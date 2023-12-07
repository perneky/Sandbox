#include "D3DDescriptorHeap.h"
#include "D3DDevice.h"
#include "D3DResourceDescriptor.h"
#include "D3DResource.h"

D3DDescriptorHeap::D3DDescriptorHeap( D3DDevice& device, int descriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE heapType, const wchar_t* debugName )
  : heapType( heapType )
{
  for ( int index = 0; index < descriptorCount; ++index )
    freeDescriptors.emplace( index );

  D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
  heapDesc.NumDescriptors = descriptorCount;
  heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  heapDesc.Type           = heapType;
  device.GetD3DDevice()->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &d3dCPUVisibleHeap ) );

  if ( debugName )
  {
    auto debugNameCpu = eastl::wstring( debugName ) + L"_CPU";
    d3dCPUVisibleHeap->SetName( debugNameCpu.data() );
  }

  if ( heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER )
  {
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device.GetD3DDevice()->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &d3dGPUVisibleHeap ) );

    if ( debugName )
    {
      auto debugNameGpu = eastl::wstring( debugName ) + L"_GPU";
      d3dGPUVisibleHeap->SetName( debugNameGpu.data() );
    }
  }

  handleSize = device.GetD3DDevice()->GetDescriptorHandleIncrementSize( heapType );

  // Fill the heap with a valid dummy descriptor, so the heap can be used as static.
  if ( heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
  {
    {
      D3D12_RESOURCE_DESC dummyDesc = {};
      dummyDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
      dummyDesc.Alignment          = 0;
      dummyDesc.Width              = 64 * 1024;
      dummyDesc.Height             = 1;
      dummyDesc.DepthOrArraySize   = 1;
      dummyDesc.MipLevels          = 1;
      dummyDesc.Format             = DXGI_FORMAT_UNKNOWN;
      dummyDesc.SampleDesc.Count   = 1;
      dummyDesc.SampleDesc.Quality = 0;
      dummyDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      dummyDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

      D3D12_HEAP_PROPERTIES dummyHeapProperties = {};
      dummyHeapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
      dummyHeapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      dummyHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

      device.GetD3DDevice()->CreateCommittedResource( &dummyHeapProperties
                                                    , D3D12_HEAP_FLAG_NONE
                                                    , &dummyDesc
                                                    , D3D12_RESOURCE_STATE_COMMON
                                                    , nullptr
                                                    , IID_PPV_ARGS( &d3dDummySRVBuffer ) );

      D3D12_SHADER_RESOURCE_VIEW_DESC dummySrvDesc = {};
      dummySrvDesc.Format                     = DXGI_FORMAT_UNKNOWN;
      dummySrvDesc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
      dummySrvDesc.Shader4ComponentMapping    = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      dummySrvDesc.Buffer.FirstElement        = 0;
      dummySrvDesc.Buffer.StructureByteStride = 4;
      dummySrvDesc.Buffer.NumElements         = UINT( dummyDesc.Width / dummySrvDesc.Buffer.StructureByteStride );
      dummySrvDesc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;

      auto dummyCpuHandle  = d3dCPUVisibleHeap->GetCPUDescriptorHandleForHeapStart(); dummyCpuHandle.ptr += handleSize * SceneBufferResourceBaseSlot;
      auto dummyGpuHandle  = d3dGPUVisibleHeap->GetCPUDescriptorHandleForHeapStart(); dummyGpuHandle.ptr += handleSize * SceneBufferResourceBaseSlot;
      auto fillerCPUHandle = dummyCpuHandle; fillerCPUHandle.ptr += handleSize;
      auto fillerGPUHandle = dummyGpuHandle; fillerGPUHandle.ptr += handleSize;

      device.GetD3DDevice()->CreateShaderResourceView( d3dDummySRVBuffer, &dummySrvDesc, dummyCpuHandle );
      device.GetD3DDevice()->CopyDescriptorsSimple( 1, dummyGpuHandle, dummyCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

      for ( int descIx = 1; descIx < SceneBufferResourceCount; ++descIx )
      {
        device.GetD3DDevice()->CopyDescriptorsSimple( 1, fillerCPUHandle, dummyCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        device.GetD3DDevice()->CopyDescriptorsSimple( 1, fillerGPUHandle, fillerCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

        fillerCPUHandle.ptr += handleSize;
        fillerGPUHandle.ptr += handleSize;
      }
    }

    {
      D3D12_RESOURCE_DESC dummyDesc = {};
      dummyDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      dummyDesc.Alignment          = 0;
      dummyDesc.Width              = 4;
      dummyDesc.Height             = 4;
      dummyDesc.DepthOrArraySize   = 1;
      dummyDesc.MipLevels          = 1;
      dummyDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
      dummyDesc.SampleDesc.Count   = 1;
      dummyDesc.SampleDesc.Quality = 0;
      dummyDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      dummyDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

      D3D12_HEAP_PROPERTIES dummyHeapProperties = {};
      dummyHeapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
      dummyHeapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      dummyHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

      device.GetD3DDevice()->CreateCommittedResource( &dummyHeapProperties
                                                    , D3D12_HEAP_FLAG_NONE
                                                    , &dummyDesc
                                                    , D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                                                    , nullptr
                                                    , IID_PPV_ARGS( &d3dDummySRVTexture ) );

      D3D12_SHADER_RESOURCE_VIEW_DESC dummySrvDesc = {};
      dummySrvDesc.Format                  = dummyDesc.Format;
      dummySrvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      dummySrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      dummySrvDesc.Texture2D.MipLevels     = 1;

      auto dummyCpuHandle  = d3dCPUVisibleHeap->GetCPUDescriptorHandleForHeapStart(); dummyCpuHandle.ptr += handleSize * Scene2DResourceBaseSlot;
      auto dummyGpuHandle  = d3dGPUVisibleHeap->GetCPUDescriptorHandleForHeapStart(); dummyGpuHandle.ptr += handleSize * Scene2DResourceBaseSlot;
      auto fillerCPUHandle = dummyCpuHandle; fillerCPUHandle.ptr += handleSize;
      auto fillerGPUHandle = dummyGpuHandle; fillerGPUHandle.ptr += handleSize;

      device.GetD3DDevice()->CreateShaderResourceView( d3dDummySRVTexture, &dummySrvDesc, dummyCpuHandle );
      device.GetD3DDevice()->CopyDescriptorsSimple( 1, dummyGpuHandle, dummyCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

      for ( int descIx = 1; descIx < Scene2DResourceCount; ++descIx )
      {
        device.GetD3DDevice()->CopyDescriptorsSimple( 1, fillerCPUHandle, dummyCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        device.GetD3DDevice()->CopyDescriptorsSimple( 1, fillerGPUHandle, fillerCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

        fillerCPUHandle.ptr += handleSize;
        fillerGPUHandle.ptr += handleSize;
      }
    }

    {
      D3D12_RESOURCE_DESC dummyDesc = {};
      dummyDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      dummyDesc.Alignment          = 0;
      dummyDesc.Width              = 4;
      dummyDesc.Height             = 4;
      dummyDesc.DepthOrArraySize   = 1;
      dummyDesc.MipLevels          = 1;
      dummyDesc.Format             = DXGI_FORMAT_R32_UINT;
      dummyDesc.SampleDesc.Count   = 1;
      dummyDesc.SampleDesc.Quality = 0;
      dummyDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      dummyDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

      D3D12_HEAP_PROPERTIES dummyHeapProperties = {};
      dummyHeapProperties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
      dummyHeapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      dummyHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

      device.GetD3DDevice()->CreateCommittedResource( &dummyHeapProperties
                                                    , D3D12_HEAP_FLAG_NONE
                                                    , &dummyDesc
                                                    , D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                                                    , nullptr
                                                    , IID_PPV_ARGS( &d3dDummyUAVTexture ) );

      D3D12_UNORDERED_ACCESS_VIEW_DESC dummyUavDesc = {};
      dummyUavDesc.Format        = DXGI_FORMAT_UNKNOWN;
      dummyUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

      auto dummyCpuHandle  = d3dCPUVisibleHeap->GetCPUDescriptorHandleForHeapStart(); dummyCpuHandle.ptr += handleSize * Scene2DFeedbackBaseSlot;
      auto dummyGpuHandle  = d3dGPUVisibleHeap->GetCPUDescriptorHandleForHeapStart(); dummyGpuHandle.ptr += handleSize * Scene2DFeedbackBaseSlot;
      auto fillerCPUHandle = dummyCpuHandle; fillerCPUHandle.ptr += handleSize;
      auto fillerGPUHandle = dummyGpuHandle; fillerGPUHandle.ptr += handleSize;

      device.GetD3DDevice()->CreateUnorderedAccessView( d3dDummyUAVTexture, nullptr, &dummyUavDesc, dummyCpuHandle );
      device.GetD3DDevice()->CopyDescriptorsSimple( 1, dummyGpuHandle, dummyCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

      for ( int descIx = 1; descIx < Scene2DResourceCount; ++descIx )
      {
        device.GetD3DDevice()->CopyDescriptorsSimple( 1, fillerCPUHandle, dummyCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        device.GetD3DDevice()->CopyDescriptorsSimple( 1, fillerGPUHandle, fillerCPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

        fillerCPUHandle.ptr += handleSize;
        fillerGPUHandle.ptr += handleSize;
      }
    }
  }
}

D3DDescriptorHeap::~D3DDescriptorHeap()
{
}

eastl::unique_ptr<ResourceDescriptor> D3DDescriptorHeap::RequestDescriptorFromSlot( Device& device, ResourceDescriptorType type, int slot, Resource& resource, int bufferElementSize, int mipLevel )
{
  assert( slot >= 0 );
  return RequestDescriptor( device, type, 0, slot, resource, bufferElementSize, mipLevel );
}

eastl::unique_ptr<ResourceDescriptor> D3DDescriptorHeap::RequestDescriptorAuto( Device& device, ResourceDescriptorType type, int base, Resource& resource, int bufferElementSize, int mipLevel )
{
  return RequestDescriptor( device, type, base, -1, resource, bufferElementSize, mipLevel );
}

eastl::unique_ptr< ResourceDescriptor > D3DDescriptorHeap::RequestDescriptor( Device& device, ResourceDescriptorType type, int base, int slot, Resource& resource, int bufferElementSize, int mipLevel )
{
  eastl::lock_guard< eastl::recursive_mutex > autoLock( descriptorLock );
  
  assert( !freeDescriptors.empty() );

  if ( slot < 0 )
    slot = *freeDescriptors.lower_bound( base );

  assert( !freeDescriptors.empty() );

  D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE d3dShaderVisibleCPUHandle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE d3dShaderVisibleGPUHandle = {};
  RequestDescriptor( slot, d3dCPUHandle, d3dShaderVisibleCPUHandle, d3dShaderVisibleGPUHandle );

  return eastl::unique_ptr< ResourceDescriptor >( new D3DResourceDescriptor( *static_cast< D3DDevice* >( &device )
                                                                           , *this
                                                                           , type
                                                                           , slot
                                                                           , *static_cast< D3DResource* >( &resource )
                                                                           , mipLevel
                                                                           , bufferElementSize
                                                                           , d3dCPUHandle
                                                                           , d3dShaderVisibleCPUHandle
                                                                           , d3dShaderVisibleGPUHandle ) );
}

int D3DDescriptorHeap::GetDescriptorSize() const
{
  return int( handleSize );
}

void D3DDescriptorHeap::FreeDescriptor( int index )
{
  eastl::lock_guard< eastl::recursive_mutex > autoLock( descriptorLock );

  freeDescriptors.emplace( index );
}

void D3DDescriptorHeap::RequestDescriptor( int slot, D3D12_CPU_DESCRIPTOR_HANDLE& d3dCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& d3dShaderVisibleCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE& d3dShaderVisibleGPUHandle )
{
  eastl::lock_guard< eastl::recursive_mutex > autoLock( descriptorLock );
  
  assert( !freeDescriptors.empty() );
  assert( slot >= 0 );

  bool isFree = freeDescriptors.count( slot ) == 1;
  assert( isFree );
  if ( !isFree )
    return;

  freeDescriptors.erase( slot );
  assert( !freeDescriptors.empty() );

  d3dCPUHandle = d3dCPUVisibleHeap->GetCPUDescriptorHandleForHeapStart();
  d3dCPUHandle.ptr += slot * handleSize;

  if ( d3dGPUVisibleHeap )
  {
    d3dShaderVisibleCPUHandle      = d3dGPUVisibleHeap->GetCPUDescriptorHandleForHeapStart();
    d3dShaderVisibleCPUHandle.ptr += slot * handleSize;
    d3dShaderVisibleGPUHandle      = d3dGPUVisibleHeap->GetGPUDescriptorHandleForHeapStart();
    d3dShaderVisibleGPUHandle.ptr += slot * handleSize;
  }
}

ID3D12DescriptorHeap* D3DDescriptorHeap::GetD3DCpuHeap()
{
  return d3dCPUVisibleHeap;
}

ID3D12DescriptorHeap* D3DDescriptorHeap::GetD3DGpuHeap()
{
  return d3dGPUVisibleHeap;
}

D3D12_DESCRIPTOR_HEAP_TYPE D3DDescriptorHeap::GetD3DHeapType()
{
  return heapType;
}
