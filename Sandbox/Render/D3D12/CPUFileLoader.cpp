#include "CPUFileLoader.h"
#include "D3DCommandList.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "D3DResourceDescriptor.h"
#include "D3DDescriptorHeap.h"
#include "D3DUtils.h"
#include "Conversion.h"
#include "../RenderManager.h"
#include "../TextureStreamers/TFFFormat.h"

eastl::unique_ptr< FileLoader > CreateFileLoader( Device& device )
{
  return eastl::make_unique< CPUFileLoader >( device );
}

static eastl::unique_ptr< Resource > LoadTileToUploadBuffer( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat )
{
  CPUSection cpuSection( L"Load single tile" );

  auto tileMemorySize = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;

  SetFilePointerEx( fileHandle, LARGE_INTEGER{ .QuadPart = byteOffset }, nullptr, FILE_BEGIN );

  auto uploadResource = device.AllocateUploadBuffer( tileMemorySize, L"TileUploadBuffer" );

  void* tileData = uploadResource->Map();

  DWORD bytesRead = 0;
  ReadFile( fileHandle, tileData, tileMemorySize, &bytesRead, nullptr );
  assert( bytesRead == tileMemorySize );

  uploadResource->Unmap();

  return uploadResource;
}

CPUFileLoader::CPUFile::CPUFile( FileLoaderQueue& loaderQueue, HANDLE fileHandle )
  : fileHandle( fileHandle )
  , loaderQueue( loaderQueue )
{
}

CPUFileLoader::CPUFile::~CPUFile()
{
  CloseHandle( fileHandle );
}

void CPUFileLoader::CPUFile::LoadPackedMipTail( Device& device
                                              , CommandList& commandList
                                              , Resource& resource
                                              , const TFFHeader& tffHeader
                                              , int blockSize )
{
  auto& d3dDevice      = static_cast< D3DDevice&      >( device      );
  auto& d3dCommandList = static_cast< D3DCommandList& >( commandList );
  auto& d3dResource    = static_cast< D3DResource&    >( resource    );

  int firstMipHBlocks = eastl::max( ( tffHeader.width  >> ( tffHeader.mipCount - tffHeader.packedMipCount ) ) / 4, 1U );
  int firstMipVBlocks = eastl::max( ( tffHeader.height >> ( tffHeader.mipCount - tffHeader.packedMipCount ) ) / 4, 1U );

  SetFilePointerEx( fileHandle, LARGE_INTEGER{ .QuadPart = sizeof( TFFHeader ) }, nullptr, FILE_BEGIN );

  eastl::vector< uint8_t > tileData( tffHeader.packedMipDataSize );
  DWORD bytesRead = 0;
  ReadFile( fileHandle, tileData.data(), DWORD( tileData.size() ), &bytesRead, nullptr );
  assert( bytesRead == tileData.size() );

  D3D12_SUBRESOURCE_DATA subresources[ 16 ];

  uint8_t* readCursor = tileData.data();
  int hblocks = firstMipHBlocks;
  int vblocks = firstMipVBlocks;
  int check   = 0;
  for ( int mip = 0; mip < int( tffHeader.packedMipCount ); ++mip )
  {
    D3D12_SUBRESOURCE_DATA& subresource = subresources[ mip ];
    subresource.pData      = readCursor;
    subresource.RowPitch   = hblocks * blockSize;
    subresource.SlicePitch = hblocks * vblocks * blockSize;

    readCursor += subresource.SlicePitch;
    hblocks = eastl::max( hblocks / 2, 1 );
    vblocks = eastl::max( vblocks / 2, 1 );

    check += int( subresource.SlicePitch );
  }

  assert( check == tffHeader.packedMipDataSize );

  D3DDeviceHelper::FillTexture( d3dCommandList, d3dDevice, d3dResource, subresources, tffHeader.packedMipCount, tffHeader.mipCount - tffHeader.packedMipCount );
}

void CPUFileLoader::CPUFile::LoadSingleTile( Device& device
                                           , CommandList& commandList
                                           , TileHeap::Allocation allocation
                                           , int byteOffset
                                           , OnTileLoadAction onTileLoadAction )
{
  loadingTiles.emplace_back( LoadingJob
    {
      .targetResource   = allocation.texture,
      .onTileLoadAction = onTileLoadAction,
      .tileX            = allocation.x,
      .tileY            = allocation.y,
    } );

  loaderQueue.Enqueue( device, fileHandle, byteOffset, allocation.texture->GetTexturePixelFormat(), loadingTiles.back().uploadResource );
}

void CPUFileLoader::CPUFile::UploadLoadedTiles( Device& device, CommandQueue& copyQueue, CommandList& commandList )
{
  using namespace std::chrono_literals;

  for ( auto iter = loadingTiles.begin(); iter != loadingTiles.end(); )
  {
    if ( iter->uploadResource )
    {
      UploadTile( device, copyQueue, commandList, eastl::move( *iter ) );
      iter = loadingTiles.erase( iter );
    }
    else
      ++iter;
  }
}

void CPUFileLoader::CPUFile::UploadTile( Device& device, CommandQueue& copyQueue, CommandList& commandList, LoadingJob&& loadingJob )
{
  GPUSection gpuSection( commandList, L"Upload single tile" );

  commandList.ChangeResourceState( *loadingJob.targetResource, ResourceStateBits::CopyDestination );

  commandList.UploadTextureRegion( std::move( loadingJob.uploadResource )
                                 , *loadingJob.targetResource
                                 , 0
                                 , loadingJob.tileX * loadingJob.targetResource->GetTextureTileWidth()
                                 , loadingJob.tileY * loadingJob.targetResource->GetTextureTileHeight()
                                 , loadingJob.targetResource->GetTextureTileWidth()
                                 , loadingJob.targetResource->GetTextureTileHeight() );

  loadingJob.onTileLoadAction( device, copyQueue, commandList );
}

CPUFileLoader::CPUFileLoader( Device& device )
{
  Start( [this]( LoadingJob& job )
  {
    job.uploadResource = LoadTileToUploadBuffer( job.device, job.fileHandle, job.byteOffset, job.pixelFormat );
  }, "CPUFileLoader" );
}

CPUFileLoader::~CPUFileLoader()
{
}

eastl::unique_ptr< FileLoaderFile > CPUFileLoader::OpenFile( const eastl::wstring& path )
{
  auto fileHandle = CreateFileW( path.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
  if ( fileHandle == INVALID_HANDLE_VALUE )
    return nullptr;

  return eastl::make_unique< CPUFile >( *this, fileHandle );
}

void CPUFileLoader::Enqueue( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat, eastl::unique_ptr< Resource >& uploadResource )
{
  AsyncJobThread::Enqueue( LoadingJob { .device = device, .fileHandle = fileHandle, .byteOffset = byteOffset, .pixelFormat = pixelFormat, .uploadResource = uploadResource } );
}
