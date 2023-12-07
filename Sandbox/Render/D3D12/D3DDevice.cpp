#include "D3DDevice.h"
#include "D3DAdapter.h"
#include "D3DCommandQueue.h"
#include "D3DCommandAllocator.h"
#include "D3DCommandList.h"
#include "D3DPipelineState.h"
#include "D3DCommandSignature.h"
#include "D3DResource.h"
#include "D3DResourceDescriptor.h"
#include "D3DDescriptorHeap.h"
#include "D3DRTBottomLevelAccelerator.h"
#include "D3DRTTopLevelAccelerator.h"
#include "D3DComputeShader.h"
#include "D3DGPUTimeQuery.h"
#include "D3DRTShaders.h"
#include "D3DUtils.h"
#include "Conversion.h"
#include "DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"
#include "Common/Files.h"
#include "../FileLoader.h"
#include "../ShaderValues.h"
#include "../ShaderStructures.h"
#include "../DearImGui/imgui_impl_dx12.h"
#include "../D3D12MemoryAllocator/D3D12MemAlloc.h"
#include "WinPixEventRuntime/pix3.h"

void EnableAftermathIfNeeded( ID3D12Device* device );

constexpr int mipmapGenHeapSize = 200;

static const GUID allocationSlot = { 1546146, 1234, 9857, { 123, 45, 23, 76, 45, 98, 56, 75 } };

D3D12MA::Allocator* globalGPUAllocator = nullptr;
void SetAllocationToD3DResource( ID3D12Resource* d3dResource, D3D12MA::Allocation* allocation )
{
  d3dResource->SetPrivateData( allocationSlot, sizeof( allocation ), &allocation );
}
D3D12MA::Allocation* GetAllocationFromD3DResource( ID3D12Resource* d3dResource )
{
  D3D12MA::Allocation* allocation = nullptr;

  UINT dataSize = sizeof( allocation );
  auto result = d3dResource->GetPrivateData( allocationSlot, &dataSize, &allocation );
  assert( SUCCEEDED( result ) && dataSize == sizeof( allocation ) );

  return allocation;
}

void Device::EnableDebugExtensions()
{
  #if 0
    CComPtr< ID3D12DeviceRemovedExtendedDataSettings > pDredSettings;
    auto dredResult = D3D12GetDebugInterface( IID_PPV_ARGS( &pDredSettings ) );
    assert( SUCCEEDED( dredResult ) );

    pDredSettings->SetAutoBreadcrumbsEnablement( D3D12_DRED_ENABLEMENT_FORCED_ON );
    pDredSettings->SetPageFaultEnablement( D3D12_DRED_ENABLEMENT_FORCED_ON );
  #endif // DEBUG_GFX_API
}

D3DDevice::D3DDevice( D3DAdapter& adapter )
{
  if FAILED( D3D12CreateDevice( adapter.GetDXGIAdapter(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS( &d3dDevice ) ) )
    return;

  EnableAftermathIfNeeded( d3dDevice );

#if DEBUG_GFX_API
  CComPtr< ID3D12InfoQueue > infoQueue;
  if SUCCEEDED( d3dDevice->QueryInterface( &infoQueue ) )
  {
    infoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE );
    infoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR,      TRUE );
    infoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_WARNING,    TRUE );

    D3D12_MESSAGE_SEVERITY severities[] =
    {
        D3D12_MESSAGE_SEVERITY_INFO
    };

    D3D12_MESSAGE_ID denyIds[] = 
    {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,
        D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
    };

    D3D12_INFO_QUEUE_FILTER newFilter = {};
    newFilter.DenyList.NumCategories = 0;
    newFilter.DenyList.pCategoryList = nullptr;
    newFilter.DenyList.NumSeverities = _countof( severities );
    newFilter.DenyList.pSeverityList = severities;
    newFilter.DenyList.NumIDs        = _countof( denyIds );
    newFilter.DenyList.pIDList       = denyIds;

    infoQueue->PushStorageFilter( &newFilter );
  }
#endif // DEBUG_GFX_API

  D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveFeatures = {};
  d3dDevice->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS1, &waveFeatures, sizeof( waveFeatures ) );
  assert( waveFeatures.WaveLaneCountMin == 32 && "Change RTXGI_DDGI_WAVE_LANE_COUNT to this value in RTXGI shader compiliation!" );

  auto getMSQualities = [&]( PixelFormat format )
  {
    for ( int i = 0; i <= maxTextureSampleCount; ++i )
    {
      D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS multisampleQualityLevels;
      multisampleQualityLevels.Format      = Convert( format );
      multisampleQualityLevels.SampleCount = i;
      d3dDevice->CheckFeatureSupport( D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &multisampleQualityLevels, sizeof( multisampleQualityLevels ) );

      msQualities[ format ][ i ] = multisampleQualityLevels.NumQualityLevels;
    }
  };

  getMSQualities( PixelFormat::RGBA8888UN );
  getMSQualities( PixelFormat::RGBA1010102UN );

  {
    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.Flags    = D3D12MA::ALLOCATOR_FLAG_NONE;
    desc.pDevice  = d3dDevice;
    desc.pAdapter = adapter.GetDXGIAdapter();

    if FAILED( D3D12MA::CreateAllocator( &desc, &allocator ) )
      return;

    globalGPUAllocator = allocator;
  }

  descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ].reset( new D3DDescriptorHeap( *this, AllResourceCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, L"ShaderResourceHeap" ) );
  descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER     ].reset( new D3DDescriptorHeap( *this, 20, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, L"SamplerHeap" ) );
  descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_RTV         ].reset( new D3DDescriptorHeap( *this, 40, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,     L"RenderTargetViewHeap" ) );
  descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_DSV         ].reset( new D3DDescriptorHeap( *this, 20, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,     L"DepthStencilViewHeap" ) );

  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = Scene2DResourceCount;
  heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  d3dDevice->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &d3dFeedbackHeap ) );

  d3dFeedbackHeap->SetName( L"FeedbackHeap" );

  if ( enableImGui )
  {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 100;
    desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    d3dDevice->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &d3dDearImGuiHeap ) );
    d3dDearImGuiHeap->SetName( L"ImGuiHeap" );

    ImGui_ImplDX12_Init( d3dDevice
                       , 3
                       , DXGI_FORMAT_R10G10B10A2_UNORM
                       , d3dDearImGuiHeap
                       , d3dDearImGuiHeap->GetCPUDescriptorHandleForHeapStart()
                       , d3dDearImGuiHeap->GetGPUDescriptorHandleForHeapStart() );

  }

  D3D12_DESCRIPTOR_HEAP_DESC mipmapGenHeapDesc = {};
  mipmapGenHeapDesc.NumDescriptors = mipmapGenHeapSize;
  mipmapGenHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  mipmapGenHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  d3dDevice->CreateDescriptorHeap( &mipmapGenHeapDesc, IID_PPV_ARGS( &d3dmipmapGenHeap ) );
  d3dmipmapGenHeap->SetName( L"MipMapGenHeap" );

  SetContainerObject( d3dDevice, this );

  auto shaderData = ReadFileToMemory( L"Content/Shaders/Downsample.cso" );
  mipmapGenComputeShader.reset( new D3DComputeShader( *this, shaderData.data(), int( shaderData.size() ), L"MipMapGen" ) );

  UpdateSamplers();
}

void D3DDevice::UpdateSamplers()
{
  D3D12_SAMPLER_DESC desc = {};
  desc.Filter         = D3D12_FILTER_ANISOTROPIC;
  desc.MaxAnisotropy  = 16;
  desc.MipLODBias     = textureLODBias;
  desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
  desc.MinLOD         = 0;
  desc.MaxLOD         = 1000;
  desc.AddressU       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  desc.AddressV       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  desc.AddressW       = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

  auto heapStart = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ]->GetD3DGpuHeap()->GetCPUDescriptorHandleForHeapStart();
  d3dDevice->CreateSampler( &desc, heapStart );

  desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  heapStart.ptr += descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ]->GetDescriptorSize();
  d3dDevice->CreateSampler( &desc, heapStart );
}

static uint64_t HashTileUploadBuffer( int width, int height, DXGI_FORMAT format )
{
  return uint64_t( width ) | ( uint64_t( height ) << 16 ) | ( uint64_t( format ) << 32 );
}

CComPtr< ID3D12Resource > D3DDevice::CreateTileUploadBuffer( ID3D12Resource* targetTexture )
{
  auto targetDesc = targetTexture->GetDesc();

  D3D12_PACKED_MIP_INFO packedMipInfo;
  D3D12_TILE_SHAPE tileShape;
  D3D12_SUBRESOURCE_TILING subresourceTiling;
  d3dDevice->GetResourceTiling( targetTexture, nullptr, &packedMipInfo, &tileShape, nullptr, 0, &subresourceTiling );

  CComPtr< ID3D12Resource > resource;

  D3D12_RESOURCE_DESC desc;
  desc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  desc.Width              = tileShape.WidthInTexels;
  desc.Height             = tileShape.HeightInTexels;
  desc.DepthOrArraySize   = 1;
  desc.MipLevels          = 1;
  desc.Format             = targetDesc.Format;
  desc.SampleDesc.Count   = 1;
  desc.SampleDesc.Quality = 0;
  desc.Layout             = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
  desc.Flags              = D3D12_RESOURCE_FLAG_NONE;
  d3dDevice->CreateReservedResource( &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &resource ) );

  resource->SetName( L"TileUploadTexture" );

  return resource;
}

D3DDevice::~D3DDevice()
{
  tileHeaps.clear();
  referenceTextures.clear();

  if ( enableImGui )
    ImGui_ImplDX12_Shutdown();

  for ( auto& heap : descriptorHeaps )
    heap.reset();

  d3dRTScartchBuffer.Release();
  d3dDearImGuiHeap.Release();
  d3dmipmapGenHeap.Release();
  mipmapGenComputeShader.reset();

  allocator->Release();
  globalGPUAllocator = allocator;
}

int D3DDevice::GetMaxSampleCountForTextures( PixelFormat format ) const
{
  auto iter = msQualities.find( format );
  if ( iter == msQualities.end() )
    return 1;

  auto& q = iter->second;
  for ( int i = maxTextureSampleCount; i > 0; --i )
  {
    if ( q[ i ] != 0 )
      return i;
  }

  // There should be at least one quality level
  assert( false );

  return 1;
}

int D3DDevice::GetMatchingSampleCountForTextures( PixelFormat format, int count ) const
{
  assert( count > 0 );
  assert( count <= maxTextureSampleCount );

  auto iter = msQualities.find( format );
  if ( iter == msQualities.end() )
    return 1;

  auto& q = iter->second;
  for ( int i = eastl::min( maxTextureSampleCount, count ); i > 0; --i )
  {
    if ( q[ i ] != 0 )
      return i;
  }

  // There should be at least one quality level
  assert( false );

  return 1;
}

int D3DDevice::GetNumberOfQualityLevelsForTextures( PixelFormat format, int samples ) const
{
  assert( samples > 0 );
  assert( samples <= maxTextureSampleCount );

  auto iter = msQualities.find( format );
  if ( iter == msQualities.end() )
    return 1;

  return iter->second[ samples ];
}

eastl::unique_ptr< CommandQueue > D3DDevice::CreateCommandQueue( CommandQueueType type )
{
  return eastl::unique_ptr< CommandQueue >( new D3DCommandQueue( *this, type ) );
}

eastl::unique_ptr< CommandAllocator > D3DDevice::CreateCommandAllocator( CommandQueueType type )
{
  return eastl::unique_ptr< CommandAllocator >( new D3DCommandAllocator( *this, type ) );
}

eastl::unique_ptr< CommandList > D3DDevice::CreateCommandList( CommandAllocator& commandAllocator, CommandQueueType queueType, uint64_t queueFrequency )
{
  return eastl::unique_ptr< CommandList >( new D3DCommandList( *this, *static_cast< D3DCommandAllocator* >( &commandAllocator ), queueType, queueFrequency ) );
}

eastl::unique_ptr< PipelineState > D3DDevice::CreatePipelineState( PipelineDesc& desc, const wchar_t* debugName )
{
  return eastl::unique_ptr< PipelineState >( new D3DPipelineState( desc, *this, debugName ) );
}

eastl::unique_ptr<CommandSignature> D3DDevice::CreateCommandSignature( CommandSignatureDesc& desc, PipelineState& pipelineState )
{
  return eastl::unique_ptr< CommandSignature >( new D3DCommandSignature( desc, *static_cast< D3DPipelineState* >( &pipelineState ), *this ) );
}

eastl::unique_ptr< Resource > D3DDevice::CreateBuffer( ResourceType resourceType, HeapType heapType, bool unorderedAccess, int size, int elementSize, const wchar_t* debugName )
{
  return eastl::unique_ptr< Resource >( new D3DResource( *this, resourceType, heapType, unorderedAccess, size, elementSize, debugName ) );
}

eastl::unique_ptr< RTBottomLevelAccelerator > D3DDevice::CreateRTBottomLevelAccelerator( CommandList& commandList, Resource& vertexBuffer, int vertexCount, int positionElementSize, int vertexStride, Resource& indexBuffer, int indexSize, int indexCount, int infoIndex, bool opaque, bool allowUpdate, bool fastBuild )
{
  return eastl::unique_ptr< RTBottomLevelAccelerator >( new D3DRTBottomLevelAccelerator( *this, *static_cast< D3DCommandList* >( &commandList ), *static_cast< D3DResource* >( &vertexBuffer ), vertexCount, positionElementSize, vertexStride, *static_cast< D3DResource* >( &indexBuffer ), indexSize, indexCount, infoIndex, opaque, allowUpdate, fastBuild ) );
}

eastl::unique_ptr< RTTopLevelAccelerator > D3DDevice::CreateRTTopLevelAccelerator( CommandList& commandList, eastl::vector< RTInstance > instances, int slot )
{
  return eastl::unique_ptr< RTTopLevelAccelerator >( new D3DRTTopLevelAccelerator( *this, *static_cast< D3DCommandList* >( &commandList ), eastl::move( instances ), slot ) );
}

eastl::unique_ptr< Resource > D3DDevice::CreateVolumeTexture( CommandList& commandList, int width, int height, int depth, const void* data, int dataSize, PixelFormat format, int slot, eastl::optional< int > uavSlot, const wchar_t* debugName )
{
  auto resource = CreateTexture( commandList, width, height, depth, 1, format, 1, 0, false, slot, uavSlot, false, debugName );

  if ( data )
  {
    auto texelSize = CalcTexelSize( format );
    D3D12_SUBRESOURCE_DATA d3dSubresource = { data, width * texelSize, width * height * texelSize };
    D3DDeviceHelper::FillTexture( *static_cast< D3DCommandList* >( &commandList ), *this, *resource, &d3dSubresource, 1, 0 );
  }

  return resource;
}

eastl::unique_ptr< Resource > D3DDevice::Create2DTexture( CommandList& commandList, int width, int height, const void* data, int dataSize, PixelFormat format, int samples, int sampleQuality, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName )
{
  auto resource = CreateTexture( commandList, width, height, 1, 1, format, samples, sampleQuality, renderable, slot, uavSlot, mipLevels, debugName );

  if ( data )
  {
    auto data8Base = static_cast< const uint8_t* >( data );
    auto data8     = data8Base;
    auto mipLevels = resource->GetTextureMipLevels();
    auto texelSize = CalcTexelSize( format );

    D3D12_SUBRESOURCE_DATA d3dSubresources[ 16 ];
    for ( int mipIx = 0; mipIx < mipLevels; ++mipIx )
    {
      d3dSubresources[ mipIx ].pData      = data8;
      d3dSubresources[ mipIx ].RowPitch   = width * texelSize;
      d3dSubresources[ mipIx ].SlicePitch = width * height * texelSize;

      assert( data8 - data8Base < dataSize );

      data8 += d3dSubresources[ mipIx ].SlicePitch;
      width  = eastl::max( width  / 2, 1 );
      height = eastl::max( height / 2, 1 );
    }

    assert( data8 - data8Base == dataSize );

    D3DDeviceHelper::FillTexture( *static_cast<D3DCommandList*>( &commandList ), *this, *resource, d3dSubresources, mipLevels, 0 );
  }

  return resource;
}

eastl::unique_ptr<Resource> D3DDevice::CreateCubeTexture( CommandList& commandList, int width, const void* data, int dataSize, PixelFormat format, bool renderable, int slot, eastl::optional<int> uavSlot, bool mipLevels, const wchar_t* debugName )
{
  assert( !data && "D3DDevice::CreateCubeTexture doesn't support initial data yet!" );

  return CreateTexture( commandList, width, width, 1, 6, format, 1, 0, renderable, slot, uavSlot, mipLevels, debugName );
}

eastl::unique_ptr<ComputeShader> D3DDevice::CreateComputeShader( const void* shaderData, int shaderSize, const wchar_t* debugName )
{
  return eastl::unique_ptr< ComputeShader >( new D3DComputeShader( *this, shaderData, shaderSize, debugName ) );
}

eastl::unique_ptr<GPUTimeQuery> D3DDevice::CreateGPUTimeQuery()
{
  return eastl::unique_ptr< GPUTimeQuery >( new D3DGPUTimeQuery( *this ) );
}

eastl::unique_ptr<RTShaders> D3DDevice::CreateRTShaders( CommandList& commandList, const eastl::vector<uint8_t>& rootSignatureShaderBinary, const eastl::vector<uint8_t>& shaderBinary, const wchar_t* rayGenEntryName, const wchar_t* missEntryName, const wchar_t* anyHitEntryName, const wchar_t* closestHitEntryName, int attributeSize, int payloadSize, int maxRecursionDepth )
{
  return eastl::unique_ptr< RTShaders >( new D3DRTShaders( *this, commandList, rootSignatureShaderBinary, shaderBinary, rayGenEntryName, missEntryName, anyHitEntryName, closestHitEntryName, attributeSize, payloadSize, maxRecursionDepth ) );
}

eastl::unique_ptr<Resource> D3DDevice::Load2DTexture( CommandList& commandList, eastl::vector< uint8_t >&& textureData, int slot, const wchar_t* debugName )
{
  CComPtr< ID3D12Resource > resourceLoader;
  std::vector< D3D12_SUBRESOURCE_DATA > d3dSubresources;
  bool isCubeMap;
  if FAILED( LoadDDSTextureFromMemory( d3dDevice, textureData.data(), textureData.size(), &resourceLoader, d3dSubresources, 0, nullptr, &isCubeMap ) )
    return nullptr;

  AllocatedResource allocatedResource = AllocatedResource( GetAllocationFromD3DResource( resourceLoader ) );

  if ( debugName )
    allocatedResource->SetName( debugName );

  assert( !isCubeMap );
  if ( isCubeMap )
    return nullptr;

  D3D12_RESOURCE_DESC desc = allocatedResource->GetDesc();
  assert( desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D );
  if( desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    return nullptr;

  eastl::unique_ptr< D3DResource > resource( new D3DResource( eastl::move( allocatedResource ), ResourceStateBits::CopyDestination ) );

  auto& heap       = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ];
  auto  descriptor = heap->RequestDescriptorFromSlot( *this, ResourceDescriptorType::ShaderResourceView, slot, *resource, 0 );
  resource->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( descriptor ) );

  D3DDeviceHelper::FillTexture( *static_cast< D3DCommandList* >( &commandList ), *this, *resource, d3dSubresources.data(), int( d3dSubresources.size() ), 0 );

  return resource;
}

eastl::unique_ptr< Resource > D3DDevice::LoadCubeTexture( CommandList& commandList, eastl::vector< uint8_t >&& textureData, int slot, const wchar_t* debugName )
{
  CComPtr< ID3D12Resource > resourceLoader;
  std::vector< D3D12_SUBRESOURCE_DATA > d3dSubresources;
  bool isCubeMap;
  if FAILED( LoadDDSTextureFromMemory( d3dDevice, textureData.data(), textureData.size(), &resourceLoader, d3dSubresources, 0, nullptr, &isCubeMap ) )
    return nullptr;

  AllocatedResource allocatedResource = AllocatedResource( GetAllocationFromD3DResource( resourceLoader ) );

  if ( debugName )
    allocatedResource->SetName( debugName );

  assert( isCubeMap );
  if ( !isCubeMap )
    return nullptr;

  auto desc = allocatedResource->GetDesc();

  eastl::unique_ptr< D3DResource > resource( new D3DResource( eastl::move( allocatedResource ), ResourceStateBits::CopyDestination ) );

  auto& heap = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ];
  auto  descriptor = heap->RequestDescriptorFromSlot( *this, ResourceDescriptorType::ShaderResourceView, slot, *resource, 0 );
  resource->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( descriptor ) );

  assert( desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D );
  if( desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    return nullptr;

  D3DDeviceHelper::FillTexture( *static_cast< D3DCommandList* >( &commandList ), *this, *resource, d3dSubresources.data(), int( d3dSubresources.size() ), 0 );

  return resource;
}

eastl::unique_ptr< Resource > D3DDevice::Stream2DTexture( CommandQueue& commandQueue
                                                        , CommandList& commandList
                                                        , const TFFHeader& tffHeader
                                                        , eastl::unique_ptr< FileLoaderFile >&& fileHandle
                                                        , int slot
                                                        , const wchar_t* debugName )
{
  D3DCommandQueue& d3dCommandQueue = static_cast< D3DCommandQueue& >( commandQueue );
  D3DCommandList&  d3dCommandList  = static_cast< D3DCommandList&  >( commandList  );

  CreateStreamingReferenceTextures( commandList );
 
  PixelFormat pixelFormat = PixelFormat::Unknown;
  switch ( tffHeader.pixelFormat )
  {
    case TFFHeader::PixelFormat::BC1: pixelFormat = PixelFormat::BC1UN; break;
    case TFFHeader::PixelFormat::BC2: pixelFormat = PixelFormat::BC2UN; break;
    case TFFHeader::PixelFormat::BC3: pixelFormat = PixelFormat::BC3UN; break;
    case TFFHeader::PixelFormat::BC4: pixelFormat = PixelFormat::BC4UN; break;
    case TFFHeader::PixelFormat::BC5: pixelFormat = PixelFormat::BC5UN; break;
    default: return nullptr;
  }

  eastl::wstring debugNameFormatter;

  auto mipTailTexture = CreateTexture( commandList
                                     , TFFHeader::tileSize
                                     , TFFHeader::tileSize
                                     , 1
                                     , 1
                                     , pixelFormat
                                     , 1
                                     , 0
                                     , false
                                     , slot + Scene2DResourceCount * 2
                                     , eastl::nullopt
                                     , true
                                     , debugNameFormatter.sprintf( L"%s_MipTail", debugName ).data() );

  auto indexWidth  = tffHeader.width  / TFFHeader::tileSize;
  auto indexHeight = tffHeader.height / TFFHeader::tileSize;

  eastl::array< uint32_t, 16 * 1024 > startingIndexData;
  auto writeCursor = startingIndexData.begin();
  auto fiw = indexWidth;
  auto fih = indexHeight;
  for ( ; fiw > 1 || fih > 1; fiw /= 2, fih /= 2 )
  {
    auto mipSize = eastl::max( fiw, 1U ) * eastl::max( fih, 1U );
    eastl::fill_n( writeCursor, mipSize, 0xFFFEFFFFU );
    writeCursor += mipSize;
  }

  eastl::fill_n( writeCursor, 1, 0xFFFF0000 | ( tffHeader.mipCount - tffHeader.packedMipCount ) );
  ++writeCursor;

  auto indexTexture = Create2DTexture( commandList
                                     , indexWidth
                                     , indexHeight
                                     , startingIndexData.data()
                                     , int( eastl::distance( startingIndexData.begin(), writeCursor ) * sizeof( uint32_t ) )
                                     , PixelFormat::RG1616U
                                     , 1
                                     , 0
                                     , false
                                     , slot
                                     , eastl::nullopt
                                     , true
                                     , debugName );

  fileHandle->LoadPackedMipTail( *this
                               , commandList
                               , *mipTailTexture
                               , sizeof( TFFHeader )
                               , tffHeader.packedMipDataSize
                               , tffHeader.packedMipCount
                               , eastl::max( ( tffHeader.width  >> ( tffHeader.mipCount - tffHeader.packedMipCount ) ) / 4, 1U )
                               , eastl::max( ( tffHeader.height >> ( tffHeader.mipCount - tffHeader.packedMipCount ) ) / 4, 1U )
                               , CalcBlockSize( mipTailTexture->GetTexturePixelFormat() ) );

  auto feedbackTexture = CreateTexture( commandList
                                      , tffHeader.width  / TFFHeader::tileSize
                                      , tffHeader.height / TFFHeader::tileSize
                                      , 1
                                      , 1
                                      , PixelFormat::R32U
                                      , 1
                                      , 0
                                      , false
                                      , -1
                                      , slot + Scene2DResourceCount
                                      , false
                                      , debugNameFormatter.sprintf( L"%s_Feedback", debugName ).data() );

  UINT clear[ 4 ] = { 255, 255, 255, 255 };
  d3dCommandList.ClearUnorderedAccess( *feedbackTexture, clear );

  auto& heap = tileHeaps[ pixelFormat ];

  return eastl::unique_ptr< Resource >( new D3DResource( *this
                                                       , eastl::move( mipTailTexture )
                                                       , eastl::move( indexTexture )
                                                       , eastl::move( feedbackTexture )
                                                       , eastl::move( fileHandle )
                                                       , int( sizeof( TFFHeader ) + tffHeader.packedMipDataSize )
                                                       , [&]( Device& device, CommandList& commandList ) { return heap->alloc( device, commandList ); }
                                                       , tffHeader.mipCount
                                                       , tffHeader.width
                                                       , tffHeader.height ) );
}

eastl::unique_ptr< Resource > D3DDevice::AllocateUploadBuffer( int dataSize, const wchar_t* resourceName )
{
  return eastl::unique_ptr< Resource >( new D3DResource( ::AllocateUploadBuffer( *this, dataSize, resourceName ), ResourceStateBits::GenericRead ) );
}

DescriptorHeap& D3DDevice::GetShaderResourceHeap()
{
  return *descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ];
}

DescriptorHeap& D3DDevice::GetSamplerHeap()
{
  return *descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ];
}

int D3DDevice::GetUploadSizeForResource( Resource& resource )
{
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
  UINT numRows;
  UINT64 rowSize;
  UINT64 totalBytes;

  auto desc = static_cast< D3DResource* >( &resource )->GetD3DResource()->GetDesc();
  d3dDevice->GetCopyableFootprints( &desc, 0, 1, 0, &layout, &numRows, &rowSize, &totalBytes );

  return int( totalBytes );
}

void D3DDevice::SetTextureLODBias( float bias )
{
  textureLODBias = bias;
  UpdateSamplers();
}

void D3DDevice::StartNewFrame()
{
  static int frame = 0;
  allocator->SetCurrentFrameIndex( ++frame );
}

void D3DDevice::CaptureNextFrames( int count )
{
#if USE_PIX
  auto result = PIXGpuCaptureNextFrames( L"GPUCapture.wpix", count );
  assert( result == S_OK );
#endif // USE_PIX
}

ID3D12Resource* D3DDevice::RequestD3DRTScartchBuffer( D3DCommandList& commandList, int size )
{
  if ( d3dRTScratchBufferSize < size )
  {
    if ( d3dRTScartchBuffer )
      commandList.HoldResource( eastl::move( d3dRTScartchBuffer ) );

    d3dRTScartchBuffer     = AllocateUAVBuffer( *this, size, ResourceStateBits::UnorderedAccess, L"RTScratch" );
    d3dRTScratchBufferSize = int( d3dRTScartchBuffer->GetDesc().Width );
  }

  return *d3dRTScartchBuffer;
}

ID3D12DeviceX* D3DDevice::GetD3DDevice()
{
  return d3dDevice;
}

ID3D12DescriptorHeap* D3DDevice::GetD3DCPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type )
{
  return descriptorHeaps[ int( type ) ]->d3dCPUVisibleHeap;
}

ID3D12DescriptorHeap* D3DDevice::GetD3DGPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type )
{
  return descriptorHeaps[ int( type ) ]->d3dGPUVisibleHeap;
}

ID3D12DescriptorHeap* D3DDevice::GetD3DDearImGuiHeap()
{
  return d3dDearImGuiHeap;
}

AllocatedResource D3DDevice::AllocateResource( HeapType heapType, const D3D12_RESOURCE_DESC& desc, ResourceState resourceState, const D3D12_CLEAR_VALUE* optimizedClearValue, bool committed )
{
  D3D12MA::Allocation* allocation = nullptr;
  ID3D12Resource2*     resource   = nullptr;

  D3D12MA::ALLOCATION_DESC allocationDesc = {};
  allocationDesc.HeapType = Convert( heapType );
  allocationDesc.Flags    = committed ? D3D12MA::ALLOCATION_FLAG_COMMITTED : D3D12MA::ALLOCATION_FLAG_NONE;

  allocator->CreateResource( &allocationDesc
                           , &desc
                           , Convert( resourceState )
                           , optimizedClearValue
                           , &allocation
                           , IID_PPV_ARGS( &resource ) );

  resource->Release(); // it is held by the allocation

  return AllocatedResource( allocation );
}

ID3D12RootSignature* D3DDevice::GetMipMapGenD3DRootSignature()
{
  return mipmapGenComputeShader->GetD3DRootSignature();
}

ID3D12PipelineState* D3DDevice::GetMipMapGenD3DPipelineState()
{
  return mipmapGenComputeShader->GetD3DPipelineState();
}

ID3D12DescriptorHeap* D3DDevice::GetMipMapGenD3DDescriptorHeap()
{
  return d3dmipmapGenHeap;
}

int D3DDevice::GetMipMapGenDescCounter()
{
  auto c = mipmapGenDescCounter;
  mipmapGenDescCounter = ++mipmapGenDescCounter % mipmapGenHeapSize;
  return c;
}

void D3DDevice::DearImGuiNewFrame()
{
  if ( enableImGui )
    ImGui_ImplDX12_NewFrame();
}

void* D3DDevice::GetDearImGuiHeap()
{
  return GetD3DDearImGuiHeap();
}

eastl::wstring D3DDevice::GetMemoryInfo( bool includeIndividualAllocations )
{
  wchar_t* stats = nullptr;
  allocator->BuildStatsString( &stats, includeIndividualAllocations );
  eastl::wstring result( stats );
  allocator->FreeStatsString( stats );
  return result;
}

eastl::unique_ptr< D3DResource > D3DDevice::CreateTexture( CommandList& commandList, int width, int height, int depth, int slices, PixelFormat format, int samples, int sampleQuality, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName, bool reserved )
{
  bool isVolumeTexture = depth  > 1;
  bool isCubeTexture   = slices > 1;

  assert( !isVolumeTexture || !isCubeTexture );

  ResourceState initialState;
  if ( IsDepthFormat( format ) )
    initialState.bits = ResourceStateBits::DepthWrite;
  if ( renderable )
    initialState.bits = ResourceStateBits::RenderTarget;
  if ( uavSlot.has_value() )
    initialState.bits = ResourceStateBits::UnorderedAccess;
  
  if ( initialState.bits == 0 )
    initialState.bits = ResourceStateBits::CopyDestination;

  auto flags = D3D12_RESOURCE_FLAG_NONE;
  if ( IsDepthFormat( format ) )
    flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  else if ( renderable )
    flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  if ( uavSlot.has_value() )
    flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  D3D12_HEAP_PROPERTIES heapProperties = {};
  heapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProperties.CreationNodeMask     = 1;
  heapProperties.VisibleNodeMask      = 1;

  D3D12_RESOURCE_DESC desc = {};
  desc.Dimension          = isVolumeTexture ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  desc.Alignment          = 0;
  desc.Width              = width;
  desc.Height             = height;
  desc.DepthOrArraySize   = eastl::max( depth, slices );
  desc.MipLevels          = mipLevels ? 0 : 1;
  desc.Format             = Convert( format );
  desc.SampleDesc.Count   = samples;
  desc.SampleDesc.Quality = sampleQuality;
  desc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  desc.Flags              = flags;

  D3D12_CLEAR_VALUE optimizedClearValue = {};
  optimizedClearValue.Format = ConvertForDSV( Convert( format ) );
  if ( IsShadowFormat( format ) )
    optimizedClearValue.DepthStencil = { 1.0f, 0 };
  else if ( IsDepthFormat( format ) )
  {
    #if USE_REVERSE_PROJECTION
      optimizedClearValue.DepthStencil = { 0.0f, 0 };
    #else
      optimizedClearValue.DepthStencil = { 1.0f, 0 };
    #endif
  }
  else if ( renderable )
  {
    optimizedClearValue.Color[ 0 ] = 0;
    optimizedClearValue.Color[ 1 ] = 0;
    optimizedClearValue.Color[ 2 ] = 0;
    optimizedClearValue.Color[ 3 ] = 0;
  }

  bool committed = ( renderable || IsDepthFormat( format ) ) && width >= 1024 && height >= 1024;

  auto allocatedResource = AllocateResource( HeapType::Default, desc, initialState, IsDepthFormat( format ) || renderable ? &optimizedClearValue : nullptr, committed );
  if ( allocatedResource )
    allocatedResource->SetName( debugName );

  eastl::unique_ptr< D3DResource > resource( new D3DResource( eastl::move( allocatedResource ), initialState ) );

  if ( slot > 0 )
  {
    auto& heap       = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ];
    auto  descriptor = heap->RequestDescriptorFromSlot( *this, ResourceDescriptorType::ShaderResourceView, slot, *resource, 0 );
    resource->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( descriptor ) );
  }

  if ( IsDepthFormat( format ) )
  {
    auto& heap       = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_DSV ];
    auto  descriptor = heap->RequestDescriptorAuto( *this, ResourceDescriptorType::DepthStencilView, 0, *resource, 0 );
    resource->AttachResourceDescriptor( ResourceDescriptorType::DepthStencilView, eastl::move( descriptor ) );
  }
  if ( renderable )
  {
    auto& heap       = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_RTV ];
    auto  descriptor = heap->RequestDescriptorAuto( *this, ResourceDescriptorType::RenderTargetView, 0, *resource, 0 );
    resource->AttachResourceDescriptor( ResourceDescriptorType::RenderTargetView, eastl::move( descriptor ) );

    if ( isCubeTexture )
    {
      for ( int slot = 1; slot < 6; ++slot )
      {
        auto namedSlot = ResourceDescriptorType( int( ResourceDescriptorType::RenderTargetView0 ) + slot );
        auto descriptor = heap->RequestDescriptorAuto( *this, namedSlot, 0, *resource, 0 );
        resource->AttachResourceDescriptor( namedSlot, eastl::move( descriptor ) );
      }
    }
  }

  if ( uavSlot.has_value() )
  {
    auto& heap = descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ];
    auto  descriptor = heap->RequestDescriptorFromSlot( *this, ResourceDescriptorType::UnorderedAccessView, *uavSlot, *resource, 0 );
    resource->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( descriptor ) );

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                 = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Transition.pResource = resource->GetD3DResource();

    static_cast< D3DCommandList* >( &commandList )->GetD3DGraphicsCommandList()->ResourceBarrier( 1, &barrier );
  }

  return resource;
}

void D3DDevice::CreateStreamingReferenceTextures( CommandList& commandList )
{
  if ( !referenceTextures.empty() )
    return;

  for ( int size = 0; size < Engine2DReferenceTextureCount; ++size )
  {
    referenceTextures.emplace_back( CreateTexture( commandList
                                                 , 4 << size
                                                 , 4 << size
                                                 , 1
                                                 , 1
                                                 , PixelFormat::BC1UN
                                                 , 1
                                                 , 0
                                                 , false
                                                 , Engine2DReferenceTextureBaseSlot + size
                                                 , eastl::nullopt
                                                 , true
                                                 , L"RefTex"
                                                 , true ) );
    commandList.ChangeResourceState( *referenceTextures.back(), ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput );
  }

  for ( auto pixelFormat : { PixelFormat::BC1UN, PixelFormat::BC2UN, PixelFormat::BC3UN, PixelFormat::BC4UN, PixelFormat::BC5UN } )
    tileHeaps[ pixelFormat ].reset( new D3DTileHeap( *this, pixelFormat, L"D3DTileHeap" ) );
}
