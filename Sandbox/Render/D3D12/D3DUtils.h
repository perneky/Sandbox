#pragma once

#include "AllocatedResource.h"
#include "Conversion.h"

inline AllocatedResource AllocateUAVBuffer( D3DDevice& device, UINT64 bufferSize, ResourceState initialResourceState = ResourceStateBits::Common, const wchar_t* resourceName = nullptr )
{
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );
  auto allocation = device.AllocateResource( HeapType::Default, bufferDesc, initialResourceState );
  if ( resourceName )
    allocation->SetName( resourceName );

  return allocation;
}

inline AllocatedResource AllocateUploadBuffer( D3DDevice& device, UINT64 datasize, const wchar_t* resourceName = nullptr )
{
  auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( datasize );
  auto allocation = device.AllocateResource( HeapType::Upload, bufferDesc, ResourceStateBits::GenericRead );
  if ( resourceName )
    allocation->SetName( resourceName );

  return allocation;
}

inline eastl::wstring GetResourceName( ID3D12Resource* resource )
{
  wchar_t name[ 1024 ];
  UINT nameSize = _countof( name );
  resource->GetPrivateData( WKPDID_D3DDebugObjectNameW, &nameSize, name );
  return name;
}

struct D3DDeviceHelper
{
  static void FillTexture( D3DCommandList& commandList, D3DDevice& d3dDevice, D3DResource& d3dResource, const D3D12_SUBRESOURCE_DATA* subresources, int numSubresources, int firstSubresource )
  {
    auto resourceDesc = d3dResource.GetD3DResource()->GetDesc();

    UINT64 intermediateSize;
    d3dDevice.GetD3DDevice()->GetCopyableFootprints( &resourceDesc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &intermediateSize );

    auto uploadResource = AllocateUploadBuffer( d3dDevice, intermediateSize );

    auto oldState = d3dResource.GetCurrentResourceState();
    commandList.ChangeResourceState( d3dResource, ResourceStateBits::CopyDestination );

    auto uploadResult = UpdateSubresources( static_cast< D3DCommandList* >( &commandList )->GetD3DGraphicsCommandList()
                                          , d3dResource.GetD3DResource()
                                          , *uploadResource
                                          , 0
                                          , firstSubresource
                                          , numSubresources
                                          , subresources );

    commandList.ChangeResourceState( d3dResource, oldState );

    assert( uploadResult != 0 );

    commandList.ChangeResourceState( d3dResource, ResourceStateBits::NonPixelShaderInput | ResourceStateBits::PixelShaderInput );

    commandList.HoldResource( eastl::unique_ptr< Resource >( new D3DResource( eastl::move( uploadResource ), ResourceStateBits::Common ) ) );
  }
};
