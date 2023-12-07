#include "CPUFileLoader.h"
#include "D3DCommandList.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "D3DResourceDescriptor.h"
#include "D3DDescriptorHeap.h"
#include "D3DUtils.h"
#include "Conversion.h"
#include "../RenderManager.h"

eastl::unique_ptr< FileLoader > CreateFileLoader( Device& device )
{
  return eastl::make_unique< CPUFileLoader >( device );
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
                                              , int byteOffset
                                              , int byteCount
                                              , int mipCount
                                              , int firstMipHBlocks
                                              , int firstMipVBlocks
                                              , int blockSize )
{
  auto& d3dDevice      = static_cast< D3DDevice&      >( device      );
  auto& d3dCommandList = static_cast< D3DCommandList& >( commandList );
  auto& d3dResource    = static_cast< D3DResource&    >( resource    );

  SetFilePointerEx( fileHandle, LARGE_INTEGER{ .QuadPart = byteOffset }, nullptr, FILE_BEGIN );

  uint8_t tileData[ TFFHeader::tileSize * TFFHeader::tileSize * 4 ];
  DWORD bytesRead = 0;
  ReadFile( fileHandle, tileData, byteCount, &bytesRead, nullptr );
  assert( bytesRead == byteCount );

  D3D12_SUBRESOURCE_DATA subresources[ 16 ];

  uint8_t* readCursor = tileData;
  int hblocks = firstMipHBlocks;
  int vblocks = firstMipVBlocks;
  for ( int mip = 0; mip < mipCount; ++mip )
  {
    D3D12_SUBRESOURCE_DATA& subresource = subresources[ mip ];
    subresource.pData      = readCursor;
    subresource.RowPitch   = hblocks * blockSize;
    subresource.SlicePitch = hblocks * vblocks * blockSize;

    readCursor += subresource.SlicePitch;
    hblocks = eastl::max( hblocks / 2, 1 );
    vblocks = eastl::max( vblocks / 2, 1 );
  }

  D3DDeviceHelper::FillTexture( d3dCommandList, d3dDevice, d3dResource, subresources, mipCount, 0 );
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

void CPUFileLoader::CPUFile::UploadLoadedTiles( Device& device, CommandList& commandList )
{
  using namespace std::chrono_literals;

  for ( auto iter = loadingTiles.begin(); iter != loadingTiles.end(); )
  {
    if ( iter->uploadResource )
    {
      UploadTile( device, commandList, eastl::move( *iter ) );
      iter = loadingTiles.erase( iter );
    }
    else
      ++iter;
  }
}

void CPUFileLoader::CPUFile::UploadTile( Device& device, CommandList& commandList, LoadingJob&& loadingJob )
{
  GPUSection gpuSection( commandList, L"Upload single tile" );

  commandList.ChangeResourceState( *loadingJob.targetResource, ResourceStateBits::CopyDestination );

  commandList.UploadTextureRegion( std::move( loadingJob.uploadResource )
                                 , *loadingJob.targetResource
                                 , 0
                                 , loadingJob.tileX * TileSizeWithBorder
                                 , loadingJob.tileY * TileSizeWithBorder
                                 , TileSizeWithBorder
                                 , TileSizeWithBorder );

  loadingJob.onTileLoadAction( device, commandList );
}

CPUFileLoader::CPUFileLoader( Device& device )
{
  InitializeCriticalSectionAndSpinCount( &queueLock, 4000 );
  hasWork = CreateEventA( nullptr, false, false, nullptr );
  loadingThread = std::thread( LoadingThreadFunc, eastl::ref( *this ) );
}

CPUFileLoader::~CPUFileLoader()
{
  EnterCriticalSection( &queueLock );
  while ( !loadingJobs.empty() )
    loadingJobs.pop();
  LeaveCriticalSection( &queueLock );

  keepLoading = false;
  SetEvent( hasWork );
  loadingThread.join();
  DeleteCriticalSection( &queueLock );
}

eastl::unique_ptr< FileLoaderFile > CPUFileLoader::OpenFile( const eastl::wstring& path )
{
  auto fileHandle = CreateFileW( path.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
  if ( fileHandle == INVALID_HANDLE_VALUE )
    return nullptr;

  return eastl::make_unique< CPUFile >( *this, fileHandle );
}

static eastl::unique_ptr< Resource > LoadTileToUploadBuffer( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat )
{
  CPUSection cpuSection( L"Load single tile" );

  auto tileMemorySize = CalcTileMemorySize( pixelFormat );

  SetFilePointerEx( fileHandle, LARGE_INTEGER{ .QuadPart = byteOffset }, nullptr, FILE_BEGIN );

  auto uploadResource = device.AllocateUploadBuffer( tileMemorySize, L"TileUploadBuffer" );

  void* tileData = uploadResource->Map();

  DWORD bytesRead = 0;
  ReadFile( fileHandle, tileData, tileMemorySize, &bytesRead, nullptr );
  assert( bytesRead == tileMemorySize );

  uploadResource->Unmap();

  return uploadResource;
}

void CPUFileLoader::Enqueue( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat, eastl::unique_ptr< Resource >& uploadResource )
{
  EnterCriticalSection( &queueLock );
  loadingJobs.emplace( LoadingJob { .device = device, .fileHandle = fileHandle, .byteOffset = byteOffset, .pixelFormat = pixelFormat, .uploadResource = uploadResource } );
  LeaveCriticalSection( &queueLock );

  SetEvent( hasWork );
}

void CPUFileLoader::LoadingThreadFunc( CPUFileLoader& loader )
{
  SetThreadName( GetCurrentThreadId(), (char*)"Tile loader" );

  while ( loader.keepLoading )
  {
    WaitForSingleObject( loader.hasWork, INFINITE );

    while ( !loader.loadingJobs.empty() )
    {
      EnterCriticalSection( &loader.queueLock );
      auto job = eastl::move( loader.loadingJobs.front() );
      loader.loadingJobs.pop();
      LeaveCriticalSection( &loader.queueLock );

      job.uploadResource = LoadTileToUploadBuffer( job.device, job.fileHandle, job.byteOffset, job.pixelFormat );
    }
  }
}
