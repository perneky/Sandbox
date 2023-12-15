#include "D3DResource.h"
#include "D3DCommandList.h"
#include "D3DDevice.h"
#include "D3DDescriptorHeap.h"
#include "D3DResourceDescriptor.h"
#include "D3DCommandQueue.h"
#include "D3DUtils.h"
#include "Conversion.h"
#include "../FileLoader.h"
#include "../ShaderStructures.h"

static int ToIndex( ResourceDescriptorType type )
{
  switch ( type )
  {
  case ResourceDescriptorType::ConstantBufferView:
  case ResourceDescriptorType::ShaderResourceView:
    return 0;

  case ResourceDescriptorType::RenderTargetView0:
    return 1;

  case ResourceDescriptorType::RenderTargetView1:
    return 2;

  case ResourceDescriptorType::RenderTargetView2:
    return 3;

  case ResourceDescriptorType::RenderTargetView3:
    return 4;

  case ResourceDescriptorType::RenderTargetView4:
    return 5;

  case ResourceDescriptorType::RenderTargetView5:
    return 6;

  case ResourceDescriptorType::DepthStencilView:
    return 7;

  case ResourceDescriptorType::UnorderedAccessView:
    return 8;

  default:
    assert( false );
    return 0;
  }
}

static ResourceType GetD3DResourceType( ID3D12Resource* d3dResource )
{
  auto desc = d3dResource->GetDesc();

  switch ( desc.Dimension )
  {
  case D3D12_RESOURCE_DIMENSION_TEXTURE1D: return ResourceType::Texture1D; break;
  case D3D12_RESOURCE_DIMENSION_TEXTURE2D: return ResourceType::Texture2D; break;
  case D3D12_RESOURCE_DIMENSION_TEXTURE3D: return ResourceType::Texture3D; break;
  case D3D12_RESOURCE_DIMENSION_BUFFER:
  default:
    return ResourceType::Buffer; break;
  }
}

void D3DResource::SetupForStreaming( eastl::unique_ptr< Resource >&& feedbackTexture
                                   , eastl::unique_ptr< FileLoaderFile >&& fileHandle
                                   , int firstMipLevelPosition
                                   , HeapAllocator heapAllocator )
{
  this->feedbackTexture       = eastl::forward< eastl::unique_ptr< Resource > >( feedbackTexture );
  this->streamingFileHandle   = eastl::forward< eastl::unique_ptr< FileLoaderFile > >( fileHandle );
  this->firstMipLevelPosition = firstMipLevelPosition;
  this->heapAllocator         = heapAllocator;

  int htiles = subresourceTiling.WidthInTiles;
  int vtiles = subresourceTiling.HeightInTiles;

  D3D12_HEAP_PROPERTIES resolvedFeedbackHeapDesc = {};
  resolvedFeedbackHeapDesc.Type                 = D3D12_HEAP_TYPE_READBACK;
  resolvedFeedbackHeapDesc.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  resolvedFeedbackHeapDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  D3D12_RESOURCE_DESC resolvedFeedbackDesc = {};
  resolvedFeedbackDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
  resolvedFeedbackDesc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  resolvedFeedbackDesc.Width              = htiles * vtiles * sizeof( uint32_t );
  resolvedFeedbackDesc.Height             = 1;
  resolvedFeedbackDesc.DepthOrArraySize   = 1;
  resolvedFeedbackDesc.MipLevels          = 1;
  resolvedFeedbackDesc.Format             = DXGI_FORMAT_UNKNOWN;
  resolvedFeedbackDesc.SampleDesc.Count   = 1;
  resolvedFeedbackDesc.SampleDesc.Quality = 0;
  resolvedFeedbackDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resolvedFeedbackDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

  CComPtr< ID3D12Device > d3dDevice;
  d3dResource->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

  CComPtr< ID3D12Resource > resolvedFeedback;
  d3dDevice->CreateCommittedResource( &resolvedFeedbackHeapDesc, D3D12_HEAP_FLAG_NONE, &resolvedFeedbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &resolvedFeedback ) );

  d3dFeedbackResolved = AllocatedResource( resolvedFeedback );

  tileMapping.resize( d3dResource->GetDesc().MipLevels );

  for ( int mipLevel = 0; mipLevel < int( tileMapping.size() ); ++mipLevel )
  {
    tileMapping[ mipLevel ].resize( htiles * vtiles );
    htiles = eastl::max( htiles / 2, 1 );
    vtiles = eastl::max( vtiles / 2, 1 );
  }
}

bool D3DResource::ManageTile( Device& device
                            , CommandQueue& graphicsQueue
                            , CommandQueue& copyQueue
                            , CommandList& commandList
                            , int tx
                            , int ty
                            , int mipLevel
                            , uint64_t frameNo
                            , int globalFeedback )
{
  auto mipsToProcess = int( packedMipInfo.NumStandardMips );

  for ( int mip = globalFeedback; mip < eastl::min( mipLevel, mipsToProcess ); ++mip )
  {
    int mipTx = tx / ( 1 << mip );
    int mipTy = ty / ( 1 << mip );
    int htiles = eastl::max( int( subresourceTiling.WidthInTiles  ) >> mip, 1 );
    int vtiles = eastl::max( int( subresourceTiling.HeightInTiles ) >> mip, 1 );

    auto& stats = tileMapping[ mip ][ mipTy * htiles + mipTx ];
    if ( !stats.allocation.tileHeap )
      continue;

    if ( stats.lastUsedFrame == frameNo - 50 )
      DropTile( device, copyQueue, commandList, stats );
  }

  for ( int mip = mipLevel; mip < mipsToProcess; ++mip )
  {
    int mipTx = tx / ( 1 << mip );
    int mipTy = ty / ( 1 << mip );
    int htiles = eastl::max( int( subresourceTiling.WidthInTiles  ) >> mip, 1 );
    int vtiles = eastl::max( int( subresourceTiling.HeightInTiles ) >> mip, 1 );

    auto& stats = tileMapping[ mip ][ mipTy * htiles + mipTx ];

    if ( stats.allocation.tileHeap )
    {
      stats.lastUsedFrame = frameNo;
      continue;
    }

    // Load data from file
    int mipStart = 0;
    for ( int cnt = 0; cnt < mip; ++cnt )
    {
      int htiles = eastl::max( int( subresourceTiling.WidthInTiles  ) >> cnt, 1 );
      int vtiles = eastl::max( int( subresourceTiling.HeightInTiles ) >> cnt, 1 );

      mipStart += D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES * htiles * vtiles;
    }

    int mipHTiles = eastl::max( int( subresourceTiling.WidthInTiles ) >> mip, 1 );
    int tileStart = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES * ( mipTy * mipHTiles + mipTx );

    stats.allocation    = heapAllocator( device, commandList );
    stats.tx            = mipTx;
    stats.ty            = mipTy;
    stats.mip           = mip;
    stats.lastUsedFrame = frameNo;
    streamingFileHandle->LoadSingleTile( device
                                       , commandList
                                       , stats.allocation
                                       , firstMipLevelPosition + mipStart + tileStart
                                       , [this, stats]( Device& device, CommandQueue& copyQueue, CommandList& commandList )
                                         {
                                           UpdateTile( device, copyQueue, commandList, stats );
                                           ++allocatedTileCount;
                                         } );
  }

  return mipLevel < mipsToProcess;
}

void D3DResource::DropTile( Device& device, CommandQueue& copyQueue, CommandList& commandList, TileStats& stats )
{
  stats.allocation.tileHeap->free( stats.allocation );
  stats.allocation.tileHeap    = nullptr;
  stats.allocation.memoryHeap  = nullptr;
  stats.allocation.texture     = nullptr;

  --allocatedTileCount;

  UpdateTile( device, copyQueue, commandList, stats );
}

void D3DResource::UpdateTile( Device& device, CommandQueue& copyQueue, CommandList& commandList, const TileStats& stats )
{
  copyQueue.UpdateTileMapping( *this, stats.tx, stats.ty, stats.mip, stats.allocation.memoryHeap, stats.allocation.y * TileCount + stats.allocation.x );
}

D3DResource::D3DResource( AllocatedResource&& allocation, ResourceState initialState )
  : resourceState( initialState )
  , d3dResource( eastl::forward< AllocatedResource >( allocation ) )
{
  auto desc = d3dResource->GetDesc();
  mipLevels = int( desc.MipLevels );
  width     = int( desc.Width );
  height    = int( desc.Height );

  resourceType = GetD3DResourceType( *d3dResource );

  isUploadResource = false; // Is this always true?

  CComPtr< ID3D12Device > d3dDevice;
  d3dResource->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

  UINT numSubresourceTilings = 1;
  d3dDevice->GetResourceTiling( *d3dResource, nullptr, &packedMipInfo, &tileShape, &numSubresourceTilings, 0, &subresourceTiling );
}

D3DResource::D3DResource( D3D12MA::Allocation* allocation, ResourceState initialState )
  : resourceState( initialState )
  , d3dResource( allocation )
{
  auto desc = d3dResource->GetDesc();
  mipLevels = int( desc.MipLevels );
  width     = int( desc.Width );
  height    = int( desc.Height );

  resourceType = GetD3DResourceType( *d3dResource );

  isUploadResource = false; // Is this always true?

  CComPtr< ID3D12Device > d3dDevice;
  d3dResource->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

  UINT numSubresourceTilings = 1;
  d3dDevice->GetResourceTiling( *d3dResource, nullptr, &packedMipInfo, &tileShape, &numSubresourceTilings, 0, &subresourceTiling );
}

D3DResource::D3DResource( D3DDevice& device, ResourceType resourceType, HeapType heapType, bool unorderedAccess, int size, int elementSize, const wchar_t* debugName )
  : resourceType( resourceType )
  , isUploadResource( heapType == HeapType::Upload )
{
  resourceState = heapType == HeapType::Upload ? ResourceStateBits::GenericRead : ( resourceType == ResourceType::IndexBuffer ? ResourceStateBits::IndexBuffer : ResourceStateBits::VertexOrConstantBuffer );
  resourceState = heapType == HeapType::Readback ? ResourceStateBits::CopyDestination : resourceState;

  if ( resourceType == ResourceType::ConstantBuffer )
  {
    size = ( size + 256 - 1 ) & -256;
    resourceType = ResourceType::Buffer;
  }

  D3D12_RESOURCE_DESC streamResourceDesc = {};
  streamResourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
  streamResourceDesc.Alignment          = 0;
  streamResourceDesc.Width              = size;
  streamResourceDesc.Height             = 1;
  streamResourceDesc.DepthOrArraySize   = 1;
  streamResourceDesc.MipLevels          = 1;
  streamResourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
  streamResourceDesc.SampleDesc.Count   = 1;
  streamResourceDesc.SampleDesc.Quality = 0;
  streamResourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  streamResourceDesc.Flags              = unorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

  d3dResource = static_cast<D3DDevice*>( &device )->AllocateResource( heapType, streamResourceDesc, resourceState );
  d3dResource->SetName( debugName );

  auto desc = d3dResource->GetDesc();
  mipLevels = int( desc.MipLevels );
  width     = int( desc.Width );
  height    = int( desc.Height );

  if ( resourceType == ResourceType::IndexBuffer )
  {
    assert( elementSize == 1 || elementSize == 2 || elementSize == 4 );
    ibView.BufferLocation = d3dResource->GetGPUVirtualAddress();
    ibView.SizeInBytes    = size;
    ibView.Format         = elementSize == 1 ? DXGI_FORMAT_R8_UINT : ( elementSize == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT );
  }
  else if ( resourceType == ResourceType::VertexBuffer )
  {
    vbView.BufferLocation = d3dResource->GetGPUVirtualAddress();
    vbView.SizeInBytes    = size;
    vbView.StrideInBytes  = elementSize;
  }
  else if ( resourceType == ResourceType::Buffer )
  {
  }
  else
    assert( false );
}

D3DResource::~D3DResource()
{
  if ( onDelete )
    onDelete();
}

void D3DResource::AttachResourceDescriptor( ResourceDescriptorType type, eastl::unique_ptr< ResourceDescriptor > descriptor )
{
  assert( !resourceDescriptors[ ToIndex( type ) ] );
  resourceDescriptors[ ToIndex( type ) ] = eastl::move( descriptor );
}

ResourceDescriptor* D3DResource::GetResourceDescriptor( ResourceDescriptorType type )
{
  return resourceDescriptors[ ToIndex( type ) ].get();
}

void D3DResource::RemoveResourceDescriptor( ResourceDescriptorType type )
{
  assert( resourceDescriptors[ ToIndex( type ) ] );
  resourceDescriptors[ ToIndex( type ) ].reset();
}

void D3DResource::RemoveAllResourceDescriptors()
{
  for ( auto& desc : resourceDescriptors )
    desc.reset();
}

ResourceState D3DResource::GetCurrentResourceState() const
{
  return resourceState;
}

ResourceType D3DResource::GetResourceType() const
{
  return resourceType;
}

bool D3DResource::IsUploadResource() const
{
  return isUploadResource;
}

int D3DResource::GetBufferSize() const
{
  return int( d3dResource->GetDesc().Width );
}

int D3DResource::GetTextureWidth() const
{
  return width;
}

int D3DResource::GetTextureHeight() const
{
  return height;
}

int D3DResource::GetTextureDepthOrArraySize() const
{
  return int( d3dResource->GetDesc().DepthOrArraySize );
}

int D3DResource::GetTextureMipLevels() const
{
  return mipLevels;
}

int D3DResource::GetTextureTileWidth() const
{
  return int( tileShape.WidthInTexels );
}

int D3DResource::GetTextureTileHeight() const
{
  return int( tileShape.HeightInTexels );
}

PixelFormat D3DResource::GetTexturePixelFormat() const
{
  auto desc = d3dResource->GetDesc();
  switch ( desc.Format )
  {
  case DXGI_FORMAT_R8_UINT:
    return PixelFormat::R8U;

  case DXGI_FORMAT_R8_UNORM:
    return PixelFormat::R8UN;

  case DXGI_FORMAT_R10G10B10A2_UNORM:
    return PixelFormat::RGBA1010102UN;

  case DXGI_FORMAT_R16G16_UINT:
    return PixelFormat::RG1616U;

  case DXGI_FORMAT_R32_TYPELESS:
    return PixelFormat::D32;
  
  case DXGI_FORMAT_BC1_UNORM:
    return PixelFormat::BC1UN;

  case DXGI_FORMAT_BC2_UNORM:
    return PixelFormat::BC2UN;

  case DXGI_FORMAT_BC3_UNORM:
    return PixelFormat::BC3UN;

  case DXGI_FORMAT_BC4_UNORM:
    return PixelFormat::BC4UN;

  case DXGI_FORMAT_BC5_UNORM:
    return PixelFormat::BC5UN;
  }

  assert( false );
  return PixelFormat::Unknown;
}

uint64_t D3DResource::GetVirtualAllocationSize() const
{
  CComPtr< ID3D12Device > d3dDevice;
  d3dResource->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

  D3D12_RESOURCE_DESC desc = d3dResource->GetDesc();

  auto allocationInfo = d3dDevice->GetResourceAllocationInfo( 0, 1, &desc );
  return allocationInfo.SizeInBytes;
}

uint64_t D3DResource::GetPhysicalAllocationSize() const
{
  if ( feedbackTexture )
  {
    uint64_t memorySize = allocatedTileCount * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES
                        + feedbackTexture->GetPhysicalAllocationSize();

    return memorySize;
  }
  else
  {
    auto desc = d3dResource->GetDesc();
    CComPtr< ID3D12Device > d3dDevice;
    d3dResource->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

    auto allocationInfo = d3dDevice->GetResourceAllocationInfo( 0, 1, &desc );
    return allocationInfo.SizeInBytes;
  }
}

void* D3DResource::Map()
{
  D3D12_RANGE readRange = { 0, 0 };
  void* bufferData = nullptr;
  d3dResource->Map( 0, &readRange, &bufferData );
  return bufferData;
}

void D3DResource::Unmap()
{
  d3dResource->Unmap( 0, nullptr );
}

void D3DResource::UploadLoadedTiles( Device& device, CommandQueue& copyQueue, CommandList& commandList )
{
  if ( streamingFileHandle )
    streamingFileHandle->UploadLoadedTiles( device, copyQueue, commandList );
}

void D3DResource::EndFeedback( CommandQueue& graphicsQueue, CommandQueue& copyQueue, Device& device, CommandList& commandList, uint64_t fence, uint64_t frameNo, int globalFeedback )
{
  if ( pendingResolveFence && !graphicsQueue.IsFenceComplete( pendingResolveFence ) )
    return;

  auto mipsToProcess = int( packedMipInfo.NumStandardMips );

  for ( int mip = 0; mip < eastl::min( globalFeedback, mipsToProcess ); ++mip )
    for ( auto& stats : tileMapping[ mip ] )
      if ( stats.allocation.texture && stats.lastUsedFrame == frameNo - 50 )
        DropTile( device, copyQueue, commandList, stats );

  if ( globalFeedback >= mipsToProcess )
    return;

  auto d3dFeedback = static_cast<D3DResource*>( feedbackTexture.get() )->GetD3DResource();

  bool needFeedbackClear = false;

  if ( pendingResolveFence )
  {
    GPUSection gpuSection( commandList, L"Tile management" );

    uint32_t* minMips;
    d3dFeedbackResolved->Map( 0, nullptr, (void**)&minMips );

    int htiles = int( subresourceTiling.WidthInTiles  );
    int vtiles = int( subresourceTiling.HeightInTiles );
    for ( int ty = 0; ty < vtiles; ++ty )
      for ( int tx = 0; tx < htiles; ++tx )
        needFeedbackClear |= ManageTile( device
                                       , graphicsQueue
                                       , copyQueue
                                       , commandList
                                       , tx
                                       , ty
                                       , minMips[ ty * htiles + tx ]
                                       , frameNo
                                       , globalFeedback );

    d3dFeedbackResolved->Unmap( 0, nullptr );

    if ( needFeedbackClear )
    {
      GPUSection gpuSection( commandList, L"Clear feedback" );

      UINT clear[ 4 ] = { 255, 255, 255, 255 };
      commandList.ClearUnorderedAccess( *feedbackTexture, clear );
    }

    pendingResolveFence = 0;
  }

  if ( needFeedbackClear )
    return;

  GPUSection gpuSection( commandList, L"Readback" );

  auto d3dCommandList = static_cast< D3DCommandList& >( commandList ).GetD3DGraphicsCommandList();

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = d3dFeedback;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  d3dCommandList->ResourceBarrier( 1, &barrier );

  auto feedbackDesc = d3dFeedback->GetDesc();

  D3D12_TEXTURE_COPY_LOCATION copySrc  = {};
  copySrc.Type       = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  copySrc.pResource  = d3dFeedback;

  D3D12_TEXTURE_COPY_LOCATION copyDest = {};
  copyDest.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  copyDest.pResource                          = *d3dFeedbackResolved;
  copyDest.PlacedFootprint.Footprint.Format   = feedbackDesc.Format;
  copyDest.PlacedFootprint.Footprint.Width    = UINT( feedbackDesc.Width );
  copyDest.PlacedFootprint.Footprint.Height   = feedbackDesc.Height;
  copyDest.PlacedFootprint.Footprint.Depth    = feedbackDesc.DepthOrArraySize;
  copyDest.PlacedFootprint.Footprint.RowPitch = UINT( feedbackDesc.Width * sizeof( uint32_t ) );
  d3dCommandList->CopyTextureRegion( &copyDest, 0, 0, 0, &copySrc, nullptr );

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  d3dCommandList->ResourceBarrier( 1, &barrier );

  pendingResolveFence = fence;
}

FileLoaderFile* D3DResource::GetLoader()
{
  return streamingFileHandle.get();
}

ID3D12Resource* D3DResource::GetD3DResource()
{
  return *d3dResource;
}

D3D12_VERTEX_BUFFER_VIEW D3DResource::GetD3DVertexBufferView()
{
  return vbView;
}

D3D12_INDEX_BUFFER_VIEW D3DResource::GetD3DIndexBufferView()
{
  return ibView;
}

D3D12_GPU_VIRTUAL_ADDRESS D3DResource::GetD3DGPUVirtualAddress()
{
  return d3dResource->GetGPUVirtualAddress();
}
