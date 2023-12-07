#include "D3DCommandList.h"
#include "D3DDevice.h"
#include "D3DCommandAllocator.h"
#include "D3DResource.h"
#include "D3DPipelineState.h"
#include "D3DRTTopLevelAccelerator.h"
#include "D3DDescriptorHeap.h"
#include "D3DResourceDescriptor.h"
#include "D3DComputeShader.h"
#include "D3DCommandSignature.h"
#include "D3DRTShaders.h"
#include "D3DUtils.h"
#include "Conversion.h"
#include "Common/Color.h"
#include "Conversion.h"
#include "../ShaderStructures.h"
#include "../ShaderValues.h"
#include "../DearImGui/imgui_impl_dx12.h"

static D3D12_SHADING_RATE Convert( VRSBlock block )
{
  switch ( block )
  {
  case VRSBlock::_1x1:
    return D3D12_SHADING_RATE_1X1;
  case VRSBlock::_1x2:
    return D3D12_SHADING_RATE_1X2;
  case VRSBlock::_2x1:
    return D3D12_SHADING_RATE_2X1;
  case VRSBlock::_2x2:
    return D3D12_SHADING_RATE_2X2;
  default:
    assert( false );
    return D3D12_SHADING_RATE_1X1;
  }
}

D3DCommandList::D3DCommandList( D3DDevice& device, D3DCommandAllocator& commandAllocator, CommandQueueType queueType, uint64_t queueFrequency )
  : device( device )
{
  device.GetD3DDevice()->CreateCommandList( 0, Convert( queueType ), commandAllocator.GetD3DCommandAllocator(), nullptr, IID_PPV_ARGS( &d3dGraphicsCommandList ) );
  frequency = queueFrequency;

  if ( queueType == CommandQueueType::Direct )
    BindHeaps();
}

D3DCommandList::~D3DCommandList()
{
}

void D3DCommandList::BindHeaps()
{
  ID3D12DescriptorHeap* allHeaps[] = { static_cast< D3DDevice* >( &device )->GetD3DGPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
                                     , static_cast< D3DDevice* >( &device )->GetD3DGPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) };
  d3dGraphicsCommandList->SetDescriptorHeaps( _countof( allHeaps ), allHeaps );
}

void D3DCommandList::AddUAVBarrier( eastl::initializer_list< eastl::reference_wrapper< Resource > > resources )
{
  D3D12_RESOURCE_BARRIER barriers[ 16 ] = {};
  assert( resources.size() <= _countof( barriers ) );

  int resIx = 0;
  for ( auto& res : resources )
  {
    barriers[ resIx ].Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[ resIx ].UAV.pResource = static_cast< D3DResource& >( res.get() ).GetD3DResource();
    resIx++;
  }

  d3dGraphicsCommandList->ResourceBarrier( UINT( resources.size() ), barriers );
}

void D3DCommandList::AddNativeUAVBarrier( eastl::initializer_list< void* > resources )
{
  D3D12_RESOURCE_BARRIER barriers[ 8 ] = {};
  assert( resources.size() <= _countof( barriers ) );

  int resIx = 0;
  for ( auto& res : resources )
  {
    barriers[ resIx ].Type                 = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[ resIx ].Transition.pResource = static_cast< ID3D12Resource* >( res );
    resIx++;
  }

  d3dGraphicsCommandList->ResourceBarrier( UINT( resources.size() ), barriers );
}

void D3DCommandList::ChangeResourceState( Resource& resource, ResourceState newState )
{
  auto d3dResource = static_cast< D3DResource* >( &resource );
  if ( d3dResource->resourceState.bits == newState.bits )
    return;

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = d3dResource->GetD3DResource();
  barrier.Transition.StateBefore = Convert( d3dResource->resourceState );
  barrier.Transition.StateAfter  = Convert( newState );
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  d3dGraphicsCommandList->ResourceBarrier( 1, &barrier );

  d3dResource->resourceState = newState;
}

void D3DCommandList::ChangeResourceState( eastl::initializer_list<ResourceStateChange> resources )
{
  D3D12_RESOURCE_BARRIER barriers[ 16 ] = {};
  assert( resources.size() <= _countof( barriers ) );

  int resIx = 0;
  for ( auto& res : resources )
  {
    auto d3dResource = static_cast<D3DResource*>( &res.resource );
    if ( d3dResource->resourceState.bits == res.newState.bits )
      continue;

    D3D12_RESOURCE_BARRIER& barrier = barriers[ resIx ];
    
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = d3dResource->GetD3DResource();
    barrier.Transition.StateBefore = Convert( d3dResource->resourceState );
    barrier.Transition.StateAfter  = Convert( res.newState );
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    d3dResource->resourceState = res.newState;

    resIx++;
  }

  if ( resIx > 0 )
    d3dGraphicsCommandList->ResourceBarrier( resIx, barriers );
}

void D3DCommandList::ClearRenderTarget( Resource& texture, const Color& color )
{
  auto d3dTexture    = static_cast< D3DResource* >( &texture );
  auto d3dDescriptor = static_cast< D3DResourceDescriptor* >( d3dTexture->GetResourceDescriptor( ResourceDescriptorType::RenderTargetView ) );
  float rgba[] = { color.r, color.g, color.b, color.a };
  d3dGraphicsCommandList->ClearRenderTargetView( d3dDescriptor->GetD3DCPUHandle(), rgba, 0, nullptr );
}

void D3DCommandList::ClearDepthStencil( Resource& texture, float depth )
{
  auto d3dTexture    = static_cast< D3DResource* >( &texture );
  auto d3dDescriptor = static_cast< D3DResourceDescriptor* >( d3dTexture->GetResourceDescriptor( ResourceDescriptorType::DepthStencilView ) );
  d3dGraphicsCommandList->ClearDepthStencilView( d3dDescriptor->GetD3DCPUHandle(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr );
}

void D3DCommandList::ClearUnorderedAccess( Resource& resource, uint32_t values[ 4 ] )
{
  auto d3dResource   = static_cast< D3DResource* >( &resource );
  auto d3dDescriptor = static_cast< D3DResourceDescriptor* >( d3dResource->GetResourceDescriptor( ResourceDescriptorType::UnorderedAccessView ) );
  d3dGraphicsCommandList->ClearUnorderedAccessViewUint( d3dDescriptor->GetD3DShaderVisibleGPUHandle(), d3dDescriptor->GetD3DCPUHandle(), d3dResource->GetD3DResource(), values, 0, nullptr );
}

void D3DCommandList::SetRenderTarget( Resource& colorTexture, Resource* depthTexture )
{
  SetRenderTarget( *colorTexture.GetResourceDescriptor( ResourceDescriptorType::RenderTargetView ), depthTexture ? depthTexture->GetResourceDescriptor( ResourceDescriptorType::DepthStencilView ) : nullptr );
}

void D3DCommandList::SetRenderTarget( const eastl::vector< Resource* >& colorTextures, Resource* depthTexture )
{
  eastl::vector< D3D12_CPU_DESCRIPTOR_HANDLE > rtDescriptors;
  for ( auto colorTexture : colorTextures )
  {
    if ( colorTexture )
    {
      auto d3dColorTexture    = static_cast< D3DResource* >( colorTexture );
      auto d3dColorDescriptor = static_cast< D3DResourceDescriptor* >( d3dColorTexture->GetResourceDescriptor( ResourceDescriptorType::RenderTargetView ) );
      rtDescriptors.emplace_back( d3dColorDescriptor->GetD3DCPUHandle() );
    }
    else
      rtDescriptors.emplace_back( D3D12_CPU_DESCRIPTOR_HANDLE { 0 } );
  }
  
  if ( depthTexture )
  {
    auto d3dDepthTexture    = static_cast< D3DResource* >( depthTexture );
    auto d3dDepthDescriptor = static_cast< D3DResourceDescriptor* >( d3dDepthTexture->GetResourceDescriptor( ResourceDescriptorType::DepthStencilView ) );

    D3D12_CPU_DESCRIPTOR_HANDLE depthDescriptor = d3dDepthDescriptor->GetD3DCPUHandle();
    d3dGraphicsCommandList->OMSetRenderTargets( UINT( rtDescriptors.size() ), rtDescriptors.data(), false, &depthDescriptor );
  }
  else
  {
    d3dGraphicsCommandList->OMSetRenderTargets( UINT( rtDescriptors.size() ), rtDescriptors.data(), false, nullptr );
  }
}

void D3DCommandList::SetRenderTarget( ResourceDescriptor& colorTextureDesciptor, ResourceDescriptor* depthTextureDesciptor )
{
  auto d3dColorDescriptor = static_cast< D3DResourceDescriptor* >( &colorTextureDesciptor );
  
  D3D12_CPU_DESCRIPTOR_HANDLE rtDescriptors[] = { d3dColorDescriptor->GetD3DCPUHandle() };
  
  if ( depthTextureDesciptor )
  {
    auto d3dDepthDescriptor = static_cast< D3DResourceDescriptor* >( depthTextureDesciptor );

    D3D12_CPU_DESCRIPTOR_HANDLE depthDescriptor = d3dDepthDescriptor->GetD3DCPUHandle();
    d3dGraphicsCommandList->OMSetRenderTargets( _countof( rtDescriptors ), rtDescriptors, false, &depthDescriptor );
  }
  else
  {
    d3dGraphicsCommandList->OMSetRenderTargets( _countof( rtDescriptors ), rtDescriptors, false, nullptr );
  }
}

void D3DCommandList::SetRenderTarget( const eastl::vector<ResourceDescriptor*>& colorTextureDesciptors, ResourceDescriptor* depthTextureDesciptor )
{
  eastl::vector< D3D12_CPU_DESCRIPTOR_HANDLE > rtDescriptors;
  for ( auto colorTextureDesciptor : colorTextureDesciptors )
  {
    auto d3dColorDescriptor = static_cast< D3DResourceDescriptor* >( colorTextureDesciptor );
    rtDescriptors.emplace_back( d3dColorDescriptor->GetD3DCPUHandle() );
  }
  
  if ( depthTextureDesciptor )
  {
    auto d3dDepthDescriptor = static_cast< D3DResourceDescriptor* >( depthTextureDesciptor );

    D3D12_CPU_DESCRIPTOR_HANDLE depthDescriptor = d3dDepthDescriptor->GetD3DCPUHandle();
    d3dGraphicsCommandList->OMSetRenderTargets( UINT( rtDescriptors.size() ), rtDescriptors.data(), false, &depthDescriptor );
  }
  else
  {
    d3dGraphicsCommandList->OMSetRenderTargets( UINT( rtDescriptors.size() ), rtDescriptors.data(), false, nullptr );
  }
}

void D3DCommandList::SetViewport( int left, int top, int width, int height )
{
  D3D12_VIEWPORT d3dViewport = { float( left ), float( top ), float( width ), float( height ), 0, 1 };
  d3dGraphicsCommandList->RSSetViewports( 1, &d3dViewport );
}

void D3DCommandList::SetScissor( int left, int top, int width, int height )
{
  D3D12_RECT d3dRect = { left, top, left + width, top + height };
  d3dGraphicsCommandList->RSSetScissorRects( 1, &d3dRect );
}

void D3DCommandList::SetPipelineState( PipelineState& pipelineState )
{
  auto d3dPipelineState = static_cast< D3DPipelineState& >( pipelineState );

  d3dGraphicsCommandList->SetPipelineState( d3dPipelineState.GetD3DPipelineState() );
  d3dGraphicsCommandList->SetGraphicsRootSignature( d3dPipelineState.GetD3DRootSignature() );
}

void D3DCommandList::SetVertexBuffer( Resource& resource )
{
  auto d3dResource   = static_cast< D3DResource* >( &resource );
  auto d3dBufferView = d3dResource->GetD3DVertexBufferView();
  d3dGraphicsCommandList->IASetVertexBuffers( 0, 1, &d3dBufferView );
}

void D3DCommandList::SetIndexBuffer( Resource& resource )
{
  auto d3dResource   = static_cast< D3DResource* >( &resource );
  auto d3dBufferView = d3dResource->GetD3DIndexBufferView();
  d3dGraphicsCommandList->IASetIndexBuffer( &d3dBufferView );
}

void D3DCommandList::SetVertexBufferToNull()
{
  d3dGraphicsCommandList->IASetVertexBuffers( 0, 1, nullptr );
}

void D3DCommandList::SetIndexBufferToNull()
{
  d3dGraphicsCommandList->IASetIndexBuffer( nullptr );
}

void D3DCommandList::SetConstantBuffer( int index, Resource& resource )
{
  auto d3dResourceDescriptor = static_cast< D3DResourceDescriptor* >( resource.GetResourceDescriptor( ResourceDescriptorType::ConstantBufferView ) );
  d3dGraphicsCommandList->SetGraphicsRootDescriptorTable( index, d3dResourceDescriptor->GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetShaderResourceView( int index, Resource& resource )
{
  auto d3dResourceDescriptor = static_cast< D3DResourceDescriptor* >( resource.GetResourceDescriptor( ResourceDescriptorType::ShaderResourceView ) );
  d3dGraphicsCommandList->SetGraphicsRootDescriptorTable( index, d3dResourceDescriptor->GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetUnorderedAccessView( int index, Resource& resource )
{
  auto d3dResourceDescriptor = static_cast< D3DResourceDescriptor* >( resource.GetResourceDescriptor( ResourceDescriptorType::UnorderedAccessView ) );
  d3dGraphicsCommandList->SetGraphicsRootDescriptorTable( index, d3dResourceDescriptor->GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetDescriptorHeap( int index, DescriptorHeap& heap, int offset )
{
  auto d3dHeap   = static_cast< D3DDescriptorHeap* >( &heap );
  auto gpuHandle = d3dHeap->GetD3DGpuHeap()->GetGPUDescriptorHandleForHeapStart();

  gpuHandle.ptr += offset * d3dHeap->GetDescriptorSize();
  d3dGraphicsCommandList->SetGraphicsRootDescriptorTable( index, gpuHandle );
}

void D3DCommandList::SetConstantValues( int index, const void* values, int numValues, int offset )
{
  d3dGraphicsCommandList->SetGraphicsRoot32BitConstants( index, numValues, values, offset );
}

void D3DCommandList::SetRayTracingScene( int index, RTTopLevelAccelerator& accelerator )
{
  auto& d3dResourceDescriptor = static_cast< D3DResourceDescriptor& >( accelerator.GetResourceDescriptor() );
  d3dGraphicsCommandList->SetGraphicsRootDescriptorTable( index, d3dResourceDescriptor.GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetPrimitiveType( PrimitiveType primitiveType )
{
  d3dGraphicsCommandList->IASetPrimitiveTopology( Convert( primitiveType ) );
}

void D3DCommandList::SetComputeShader( ComputeShader& shader)
{
  auto d3dComputeShader = static_cast< D3DComputeShader* >( &shader );

  d3dGraphicsCommandList->SetPipelineState( d3dComputeShader->GetD3DPipelineState() );
  d3dGraphicsCommandList->SetComputeRootSignature( d3dComputeShader->GetD3DRootSignature() );
}

void D3DCommandList::SetComputeConstantValues( int index, const void* values, int numValues, int offset )
{
  d3dGraphicsCommandList->SetComputeRoot32BitConstants( index, numValues, values, offset );
}

void D3DCommandList::SetComputeConstantBuffer( int index, Resource& resource )
{
  auto d3dResourceDescriptor = static_cast< D3DResourceDescriptor* >( resource.GetResourceDescriptor( ResourceDescriptorType::ConstantBufferView ) );
  d3dGraphicsCommandList->SetComputeRootDescriptorTable( index, d3dResourceDescriptor->GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetComputeShaderResourceView( int index, Resource& resource )
{
  auto d3dResourceDescriptor = static_cast< D3DResourceDescriptor* >( resource.GetResourceDescriptor( ResourceDescriptorType::ShaderResourceView ) );
  d3dGraphicsCommandList->SetComputeRootDescriptorTable( index, d3dResourceDescriptor->GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetComputeUnorderedAccessView( int index, Resource& resource )
{
  auto d3dResourceDescriptor = static_cast< D3DResourceDescriptor* >( resource.GetResourceDescriptor( ResourceDescriptorType::UnorderedAccessView ) );
  d3dGraphicsCommandList->SetComputeRootDescriptorTable( index, d3dResourceDescriptor->GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetComputeRayTracingScene( int index, RTTopLevelAccelerator& accelerator )
{
  auto& d3dResourceDescriptor = static_cast< D3DResourceDescriptor& >( accelerator.GetResourceDescriptor() );
  d3dGraphicsCommandList->SetComputeRootDescriptorTable( index, d3dResourceDescriptor.GetD3DShaderVisibleGPUHandle() );
}

void D3DCommandList::SetComputeDescriptorHeap( int index, DescriptorHeap& heap, int offset )
{
  auto d3dHeap   = static_cast< D3DDescriptorHeap* >( &heap );
  auto gpuHandle = d3dHeap->GetD3DGpuHeap()->GetGPUDescriptorHandleForHeapStart();

  gpuHandle.ptr += offset * d3dHeap->GetDescriptorSize();
  d3dGraphicsCommandList->SetComputeRootDescriptorTable( index, gpuHandle );
}

void D3DCommandList::SetVariableRateShading( VRSBlock block )
{
  d3dGraphicsCommandList->RSSetShadingRate( Convert( block ), nullptr );
}

void D3DCommandList::Dispatch( int groupsX, int groupsY, int groupsZ )
{
  d3dGraphicsCommandList->Dispatch( groupsX, groupsY, groupsZ );
}

void D3DCommandList::SetRayTracingShader( RTShaders& shaders )
{
  auto d3dShaders = static_cast< D3DRTShaders* >( &shaders );

  d3dGraphicsCommandList->SetComputeRootSignature( d3dShaders->GetD3DRootSignature() );
  d3dGraphicsCommandList->SetPipelineState1( d3dShaders->GetD3DStateObject() );

  rayDesc.RayGenerationShaderRecord = d3dShaders->GetRayGenerationShaderRecord();
  rayDesc.MissShaderTable           = d3dShaders->GetMissShaderTable();
  rayDesc.HitGroupTable             = d3dShaders->GetHitGroupTable();
}

void D3DCommandList::DispatchRays( int width, int height, int depth )
{
  rayDesc.Width  = width;
  rayDesc.Height = height;
  rayDesc.Depth  = depth;
  d3dGraphicsCommandList->DispatchRays( &rayDesc );
}

void D3DCommandList::ExecuteIndirect( CommandSignature& commandSignature, Resource& argsBuffer, int argsOffset, Resource& countBuffer, int countOffset, int maximumCount )
{
  auto d3dCommandSignature = static_cast< D3DCommandSignature* >( &commandSignature );
  auto d3dArgsBuffer       = static_cast< D3DResource*         >( &argsBuffer );
  auto d3dCountBuffer      = static_cast< D3DResource*         >( &countBuffer );
  d3dGraphicsCommandList->ExecuteIndirect( d3dCommandSignature->GetD3DCommandSignature(), maximumCount, d3dArgsBuffer->GetD3DResource(), argsOffset, d3dCountBuffer->GetD3DResource(), countOffset );
}

void D3DCommandList::GenerateMipmaps( Resource& resource )
{
  auto d3dResource = static_cast< D3DResource* >( &resource )->GetD3DResource();
  auto desc = d3dResource->GetDesc();
  if ( desc.MipLevels == 1 )
    return;

  CComPtr< ID3D12Device8 > d3dDevice;
  d3dGraphicsCommandList->GetDevice( IID_PPV_ARGS( &d3dDevice ) );

  auto device = GetContainerObject< D3DDevice >( d3dDevice );
  auto mipmapGenSig  = device->GetMipMapGenD3DRootSignature();
  auto mipmapGenPSO  = device->GetMipMapGenD3DPipelineState();
  auto mipmapGenHeap = device->GetMipMapGenD3DDescriptorHeap();

  d3dGraphicsCommandList->SetPipelineState( mipmapGenPSO );
  d3dGraphicsCommandList->SetComputeRootSignature( mipmapGenSig );
  d3dGraphicsCommandList->SetDescriptorHeaps( 1, &mipmapGenHeap );

  auto descriptorSize = d3dDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
  D3D12_CPU_DESCRIPTOR_HANDLE currentCPUHandle = mipmapGenHeap->GetCPUDescriptorHandleForHeapStart();
  D3D12_GPU_DESCRIPTOR_HANDLE currentGPUHandle = mipmapGenHeap->GetGPUDescriptorHandleForHeapStart();

  auto oldState = resource.GetCurrentResourceState();
  ChangeResourceState( resource, ResourceStateBits::UnorderedAccess );

  for ( int srcMip = 0; srcMip < desc.MipLevels - 1; ++srcMip )
  {
    auto dstWidth  = eastl::max( int( desc.Width  ) >> ( srcMip + 1 ), 1 );
    auto dstHeight = eastl::max( int( desc.Height ) >> ( srcMip + 1 ), 1 );

    auto srcSlot = device->GetMipMapGenDescCounter();
    auto dstSlot = device->GetMipMapGenDescCounter();

    auto srcCPULoc = currentCPUHandle;
    auto dstCPULoc = currentCPUHandle;
    auto srcGPULoc = currentGPUHandle;
    auto dstGPULoc = currentGPUHandle;
    
    srcCPULoc.ptr += descriptorSize * srcSlot;
    dstCPULoc.ptr += descriptorSize * dstSlot;
    srcGPULoc.ptr += descriptorSize * srcSlot;
    dstGPULoc.ptr += descriptorSize * dstSlot;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = d3dResource;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = srcMip;
    d3dGraphicsCommandList->ResourceBarrier( 1, &barrier );

    D3D12_SHADER_RESOURCE_VIEW_DESC srcTextureSRVDesc = {};
    srcTextureSRVDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srcTextureSRVDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
    srcTextureSRVDesc.Format                    = desc.Format;
    srcTextureSRVDesc.Texture2D.MipLevels       = 1;
    srcTextureSRVDesc.Texture2D.MostDetailedMip = srcMip;
    d3dDevice->CreateShaderResourceView( d3dResource, &srcTextureSRVDesc, srcCPULoc );

    D3D12_UNORDERED_ACCESS_VIEW_DESC destTextureUAVDesc = {};
    destTextureUAVDesc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
    destTextureUAVDesc.Format             = desc.Format;
    destTextureUAVDesc.Texture2D.MipSlice = srcMip + 1;
    d3dDevice->CreateUnorderedAccessView( d3dResource, nullptr, &destTextureUAVDesc, dstCPULoc );

    d3dGraphicsCommandList->SetComputeRootDescriptorTable( 0, srcGPULoc );
    d3dGraphicsCommandList->SetComputeRootDescriptorTable( 1, dstGPULoc );

    d3dGraphicsCommandList->Dispatch( ( dstWidth  + DownsamplingKernelWidth  - 1 ) / DownsamplingKernelWidth
                                    , ( dstHeight + DownsamplingKernelHeight - 1 ) / DownsamplingKernelHeight
                                    , 1 );

    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = d3dResource;
    d3dGraphicsCommandList->ResourceBarrier( 1, &uavBarrier );
  }

  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = d3dResource;
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  barrier.Transition.Subresource = desc.MipLevels - 1;
  d3dGraphicsCommandList->ResourceBarrier( 1, &barrier );

  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  d3dGraphicsCommandList->ResourceBarrier( 1, &barrier );

  ID3D12DescriptorHeap* allHeaps[] = { device->GetD3DGPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV )
                                     , device->GetD3DGPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ) };
  d3dGraphicsCommandList->SetDescriptorHeaps( _countof( allHeaps ), allHeaps );
}

void D3DCommandList::DearImGuiRender()
{
  auto d3dHeap = static_cast< D3DDevice* >( &device )->GetD3DDearImGuiHeap();
  d3dGraphicsCommandList->SetDescriptorHeaps( 1, &d3dHeap );
  ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), d3dGraphicsCommandList );
}

void D3DCommandList::Draw( int vertexCount, int instanceCount, int startVertex, int startInstance )
{
  d3dGraphicsCommandList->DrawInstanced( vertexCount, instanceCount, startVertex, startInstance );
}

void D3DCommandList::DrawIndexed( int indexCount, int instanceCount, int startIndex, int baseVertex, int startInstance )
{
  d3dGraphicsCommandList->DrawIndexedInstanced( indexCount, instanceCount, startIndex, baseVertex, startInstance );
}

void D3DCommandList::UploadTextureResource( eastl::unique_ptr< Resource > source, Resource& destination, const void* data, int stride, int rows )
{
  auto oldState = destination.GetCurrentResourceState();
  ChangeResourceState( destination, ResourceStateBits::CopyDestination );

  auto d3dSource      = static_cast< D3DResource* >( source.get() )->GetD3DResource();
  auto d3dDestination = static_cast< D3DResource* >( &destination )->GetD3DResource();

  D3D12_SUBRESOURCE_DATA subresource;
  subresource.pData      = data;
  subresource.RowPitch   = stride;
  subresource.SlicePitch = stride * rows;

  UpdateSubresources( d3dGraphicsCommandList, d3dDestination, d3dSource, 0, 0, 1, &subresource );

  ChangeResourceState( destination, oldState );

  heldResources.emplace_back( eastl::move( source ) );
}

void D3DCommandList::UploadTextureRegion( eastl::unique_ptr< Resource > source, Resource& destination, int mip, int left, int top, int width, int height )
{
  auto& d3dTargetResource = static_cast< D3DResource& >( destination );
  auto& d3dUploadResource = static_cast< D3DResource& >( *source );

  auto format = d3dTargetResource.GetTexturePixelFormat();

  D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
  srcLocation.pResource                          = d3dUploadResource.GetD3DResource();
  srcLocation.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  srcLocation.PlacedFootprint.Footprint.Format   = d3dTargetResource.GetD3DResource()->GetDesc().Format;
  srcLocation.PlacedFootprint.Footprint.Width    = width;
  srcLocation.PlacedFootprint.Footprint.Height   = height;
  srcLocation.PlacedFootprint.Footprint.Depth    = 1;
  srcLocation.PlacedFootprint.Footprint.RowPitch = IsBlockFormat( format ) ? ( width / 4 ) * CalcBlockSize( format ) : width * CalcTexelSize( format );

  D3D12_TEXTURE_COPY_LOCATION dstLocation;
  dstLocation.pResource        = d3dTargetResource.GetD3DResource();
  dstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLocation.SubresourceIndex = mip;

  d3dGraphicsCommandList->CopyTextureRegion( &dstLocation
                                           , left
                                           , top
                                           , 0
                                           , &srcLocation
                                           , nullptr );
  
  HoldResource( eastl::move( source ) );
}

void D3DCommandList::UploadBufferResource( eastl::unique_ptr< Resource > source, Resource& destination, const void* data, int dataSize )
{
  auto oldState = destination.GetCurrentResourceState();
  ChangeResourceState( destination, ResourceStateBits::CopyDestination );

  auto d3dSource      = static_cast< D3DResource* >( source.get() )->GetD3DResource();
  auto d3dDestination = static_cast< D3DResource* >( &destination )->GetD3DResource();

  D3D12_SUBRESOURCE_DATA subresource;
  subresource.pData      = data;
  subresource.RowPitch   = dataSize;
  subresource.SlicePitch = dataSize;

  auto uploadSize = source->GetBufferSize();
  eastl::vector< uint8_t > paddedUploadBuffer;
  if ( dataSize < uploadSize )
  {
    paddedUploadBuffer.resize( uploadSize );
    memcpy_s( paddedUploadBuffer.data(), paddedUploadBuffer.size(), data, dataSize );
    subresource.pData      = paddedUploadBuffer.data();
    subresource.RowPitch   = paddedUploadBuffer.size();
    subresource.SlicePitch = paddedUploadBuffer.size();
  }

  UpdateSubresources( d3dGraphicsCommandList, d3dDestination, d3dSource, 0, 0, 1, &subresource );

  ChangeResourceState( destination, oldState );

  heldResources.emplace_back( eastl::move( source ) );
}

void D3DCommandList::UpdateBufferRegion( eastl::unique_ptr< Resource > source, Resource& destination, int offset )
{
  auto d3dSource      = static_cast< D3DResource* >( source.get() )->GetD3DResource();
  auto d3dDestination = static_cast< D3DResource* >( &destination )->GetD3DResource();

  auto oldState = destination.GetCurrentResourceState();

  ChangeResourceState( { { destination, ResourceStateBits::CopyDestination }
                       , { *source,     ResourceStateBits::CopySource      } } );

  d3dGraphicsCommandList->CopyBufferRegion( d3dDestination, offset, d3dSource, 0, source->GetBufferSize() );

  ChangeResourceState( destination, oldState );

  heldResources.emplace_back( eastl::move( source ) );
}

void D3DCommandList::CopyResource( Resource& source, Resource& destination )
{
  auto oldDstState = destination.GetCurrentResourceState();
  auto oldSrcState = source.GetCurrentResourceState();

  ChangeResourceState( { { destination, ResourceStateBits::CopyDestination }
                       , { source,      ResourceStateBits::CopySource      } } );

  d3dGraphicsCommandList->CopyResource( static_cast< D3DResource* >( &destination )->GetD3DResource(), static_cast< D3DResource* >( &source )->GetD3DResource() );

  ChangeResourceState( { { destination, oldDstState }
                       , { source,      oldSrcState } } );
}

void D3DCommandList::ResolveMSAA( Resource& source, Resource& destination )
{
  auto oldDstState = destination.GetCurrentResourceState();
  auto oldSrcState = source.GetCurrentResourceState();

  ChangeResourceState( { { destination, ResourceStateBits::ResolveDestination }
                       , { source,      ResourceStateBits::ResolveSource      } } );

  d3dGraphicsCommandList->ResolveSubresource( static_cast< D3DResource* >( &destination )->GetD3DResource(), 0, static_cast< D3DResource* >( &source )->GetD3DResource(), 0, Convert( source.GetTexturePixelFormat() ) );

  ChangeResourceState( { { destination, oldDstState }
                       , { source,      oldSrcState } } );
}

void D3DCommandList::HoldResource( eastl::unique_ptr< Resource > resource )
{
  if ( resource )
    heldResources.emplace_back( eastl::move( resource ) );
}

void D3DCommandList::HoldResource( eastl::unique_ptr< RTTopLevelAccelerator > resource )
{
  if ( resource )
    heldTLAS.emplace_back( eastl::move( resource ) );
}

void D3DCommandList::HoldResource( IUnknown* unknown )
{
  if ( unknown )
    heldUnknowns.emplace_back( unknown );
}

eastl::vector< eastl::unique_ptr< Resource > > D3DCommandList::TakeHeldResources()
{
  return eastl::move( heldResources );
}

eastl::vector<eastl::unique_ptr<RTTopLevelAccelerator>> D3DCommandList::TakeHeldTLAS()
{
  return eastl::move( heldTLAS );
}

eastl::vector< CComPtr< IUnknown > > D3DCommandList::TakeHeldUnknowns()
{
  return eastl::move( heldUnknowns );
}

void D3DCommandList::BeginEvent( const wchar_t* format, ... )
{
#if USE_PIX
  wchar_t msg[ 512 ];

  va_list args;
  va_start( args, format );
  vswprintf_s( msg, format, args );
  va_end( args );

  PIXBeginEvent( (ID3D12GraphicsCommandList6*)d3dGraphicsCommandList, PIX_COLOR_DEFAULT, msg );
#endif // USE_PIX
}

void D3DCommandList::EndEvent()
{
#if USE_PIX
  PIXEndEvent( (ID3D12GraphicsCommandList6*)d3dGraphicsCommandList );
#endif // USE_PIX
}

void D3DCommandList::RegisterEndFrameCallback( EndFrameCallback&& callback )
{
  endFrameCallbacks.emplace_back( eastl::move( callback ) );
}

eastl::vector< CommandList::EndFrameCallback > D3DCommandList::TakeEndFrameCallbacks()
{
  return eastl::move( endFrameCallbacks );
}

void D3DCommandList::HoldResource( D3D12MA::Allocation* allocation )
{
  HoldResource( eastl::unique_ptr< Resource >( new D3DResource( allocation, ResourceStateBits::Common ) ) );
}

void D3DCommandList::HoldResource( AllocatedResource&& allocation )
{
  HoldResource( eastl::unique_ptr< Resource >( new D3DResource( eastl::forward< AllocatedResource >( allocation ), ResourceStateBits::Common ) ) );
}

ID3D12GraphicsCommandList6* D3DCommandList::GetD3DGraphicsCommandList()
{
  return d3dGraphicsCommandList;
}

uint64_t D3DCommandList::GetFrequency() const
{
  return frequency;
}
