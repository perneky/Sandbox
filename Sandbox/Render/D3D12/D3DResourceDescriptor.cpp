#include "D3DResourceDescriptor.h"
#include "D3DDescriptorHeap.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "Conversion.h"

D3DResourceDescriptor::D3DResourceDescriptor( D3DDevice& device
                                            , D3DDescriptorHeap& heap
                                            , ResourceDescriptorType type
                                            , int slot
                                            , D3DResource& resource
                                            , int mipLevel
                                            , int bufferElementSize
                                            , D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle
                                            , D3D12_CPU_DESCRIPTOR_HANDLE d3dShaderVisibleCPUHandle
                                            , D3D12_GPU_DESCRIPTOR_HANDLE d3dShaderVisibleGPUHandle )
  : d3dCPUHandle( d3dCPUHandle )
  , d3dShaderVisibleCPUHandle( d3dShaderVisibleCPUHandle )
  , d3dShaderVisibleGPUHandle( d3dShaderVisibleGPUHandle )
  , unregister( [&heap, slot]() { heap.FreeDescriptor( slot ); } )
  , slot( slot )
{
  auto resourceDesc = resource.GetD3DResource()->GetDesc();

  switch ( type )
  {
  case ResourceDescriptorType::Typeless:
    break;

  case ResourceDescriptorType::ConstantBufferView:
  {
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
    desc.BufferLocation = resource.GetD3DResource()->GetGPUVirtualAddress();
    desc.SizeInBytes    = UINT( resource.GetD3DResource()->GetDesc().Width );
    device.GetD3DDevice()->CreateConstantBufferView( &desc, d3dCPUHandle );
    break;
  }

  case ResourceDescriptorType::ShaderResourceView:
  {
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format                  = ConvertForSRV( resourceDesc.Format );
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch ( resourceDesc.Dimension )
    {
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
      if ( resourceDesc.SampleDesc.Count > 1 )
      {
        if ( resourceDesc.DepthOrArraySize == 1 )
          desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
        else
          desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
        desc.Texture2D.MipLevels = -1;
      }
      else
      {
        if ( resourceDesc.DepthOrArraySize == 1 )
          desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        else
          desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        desc.Texture2D.MipLevels = -1;
      }
      break;

    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
      desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      desc.Texture3D.MipLevels = 1;
      break;

    case D3D12_RESOURCE_DIMENSION_BUFFER:
      if ( bufferElementSize == 0 )
      {
        desc.ViewDimension                            = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        desc.RaytracingAccelerationStructure.Location = resource.GetD3DResource()->GetGPUVirtualAddress();
      }
      else
      {
        desc.ViewDimension              = D3D12_SRV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement        = 0;
        desc.Buffer.StructureByteStride = bufferElementSize;
        desc.Buffer.NumElements         = UINT( resourceDesc.Width / bufferElementSize );
        desc.Buffer.Flags               = D3D12_BUFFER_SRV_FLAG_NONE;
      }
      break;

    default:
      assert( false );
      break;
    }

    if ( desc.ViewDimension == D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE )
      device.GetD3DDevice()->CreateShaderResourceView( nullptr, &desc, d3dCPUHandle );
    else
      device.GetD3DDevice()->CreateShaderResourceView( resource.GetD3DResource(), &desc, d3dCPUHandle );
    break;
  }

  case ResourceDescriptorType::RenderTargetView0:
  case ResourceDescriptorType::RenderTargetView1:
  case ResourceDescriptorType::RenderTargetView2:
  case ResourceDescriptorType::RenderTargetView3:
  case ResourceDescriptorType::RenderTargetView4:
  case ResourceDescriptorType::RenderTargetView5:
  {
    D3D12_RENDER_TARGET_VIEW_DESC desc = {};
    desc.Format = resourceDesc.Format;
    if ( resourceDesc.DepthOrArraySize > 1 )
    {
      int arrayIndex = int( type ) - int( ResourceDescriptorType::RenderTargetView0 );
      if ( resourceDesc.SampleDesc.Count > 1 )
      {
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
        desc.Texture2DMSArray.FirstArraySlice = arrayIndex;
        desc.Texture2DMSArray.ArraySize = 1;
      }
      else
      {
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.FirstArraySlice = arrayIndex;
        desc.Texture2DArray.ArraySize = 1;
      }
    }
    else
      desc.ViewDimension = resourceDesc.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
    device.GetD3DDevice()->CreateRenderTargetView( resource.GetD3DResource(), &desc, d3dCPUHandle );
    break;
  }

  case ResourceDescriptorType::DepthStencilView:
  {
    D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
    desc.Format               = ConvertForDSV( resourceDesc.Format );
    desc.ViewDimension        = resourceDesc.SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
    device.GetD3DDevice()->CreateDepthStencilView( resource.GetD3DResource(), &desc, d3dCPUHandle );
    break;
  }

  case ResourceDescriptorType::UnorderedAccessView:
  {
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
    desc.Format = resourceDesc.Format;
    if ( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
    {
      desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipSlice = mipLevel;
    }
    else if ( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D )
    {
      desc.ViewDimension   = D3D12_UAV_DIMENSION_TEXTURE3D;
      desc.Texture3D.WSize = resourceDesc.DepthOrArraySize;
    }
    else if ( resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER )
    {
      desc.ViewDimension              = D3D12_UAV_DIMENSION_BUFFER;
      desc.Buffer.StructureByteStride = bufferElementSize;
      desc.Buffer.NumElements         = bufferElementSize ? UINT( resourceDesc.Width / bufferElementSize ) : UINT( resourceDesc.Width / sizeof( uint32_t ) );
      desc.Buffer.Flags               = bufferElementSize ? D3D12_BUFFER_UAV_FLAG_NONE : D3D12_BUFFER_UAV_FLAG_RAW;

      if ( !bufferElementSize )
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
    }
    else
      assert( false );

    device.GetD3DDevice()->CreateUnorderedAccessView( resource.GetD3DResource(), nullptr, &desc, d3dCPUHandle );
    break;
  }
  }

  if ( d3dShaderVisibleCPUHandle.ptr )
    device.GetD3DDevice()->CopyDescriptorsSimple( 1, d3dShaderVisibleCPUHandle, d3dCPUHandle, heap.GetD3DHeapType() );
}

D3DResourceDescriptor::D3DResourceDescriptor( D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle
                                            , D3D12_CPU_DESCRIPTOR_HANDLE d3dShaderVisibleCPUHandle
                                            , D3D12_GPU_DESCRIPTOR_HANDLE d3dShaderVisibleGPUHandle
                                            , D3DDescriptorHeap& heap
                                            , int slot )
  : d3dCPUHandle( d3dCPUHandle )
  , d3dShaderVisibleCPUHandle( d3dShaderVisibleCPUHandle )
  , d3dShaderVisibleGPUHandle( d3dShaderVisibleGPUHandle )
  , unregister( [&heap, slot]() { heap.FreeDescriptor( slot ); } )
  , slot( slot )
{
}

D3DResourceDescriptor::D3DResourceDescriptor( D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle )
  : d3dCPUHandle( d3dCPUHandle )
{
}

D3DResourceDescriptor::~D3DResourceDescriptor()
{
}

int D3DResourceDescriptor::GetSlot() const
{
  assert( slot > -1 );
  return slot;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DResourceDescriptor::GetD3DCPUHandle() const
{
  return d3dCPUHandle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DResourceDescriptor::GetD3DShaderVisibleCPUHandle() const
{
  return d3dShaderVisibleCPUHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3DResourceDescriptor::GetD3DShaderVisibleGPUHandle() const
{
  return d3dShaderVisibleGPUHandle;
}
