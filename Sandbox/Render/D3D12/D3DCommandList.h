#pragma once

#include "../CommandList.h"
#include "../Types.h"

class D3DCommandAllocator;
struct AllocatedResource;

namespace D3D12MA { class Allocation; }

class D3DCommandList : public CommandList
{
  friend class D3DDevice;

public:
  ~D3DCommandList();

  void BindHeaps() override;

  void ChangeResourceState( Resource& resource, ResourceState newState ) override;
  void ChangeResourceState( eastl::initializer_list< ResourceStateChange > resources ) override;

  void ClearRenderTarget( Resource& texture, const Color& color ) override;
  void ClearDepthStencil( Resource& texture, float depth ) override;
  void ClearUnorderedAccess( Resource& resource, uint32_t values[ 4 ] ) override;

  void SetPipelineState( PipelineState& pipelineState ) override;
  void SetComputeShader( ComputeShader& shader ) override;
  void SetRayTracingShader( RTShaders& shaders ) override;
  
  void SetRenderTarget( Resource& colorTexture, Resource* depthTexture ) override;
  void SetRenderTarget( const eastl::vector< Resource* >& colorTextures, Resource* depthTexture ) override;
  void SetRenderTarget( ResourceDescriptor& colorTextureDesciptor, ResourceDescriptor* depthTextureDesciptor ) override;
  void SetRenderTarget( const eastl::vector< ResourceDescriptor* >& colorTextureDesciptors, ResourceDescriptor* depthTextureDesciptor ) override;
  void SetViewport( int left, int top, int width, int height ) override;
  void SetScissor( int left, int top, int width, int height ) override;
  void SetVertexBuffer( Resource& resource ) override;
  void SetIndexBuffer( Resource& resource ) override;
  void SetVertexBufferToNull() override;
  void SetIndexBufferToNull() override;
  void SetConstantBuffer( int index, Resource& resource ) override;
  void SetShaderResourceView( int index, Resource& resource ) override;
  void SetUnorderedAccessView( int index, Resource& resource ) override;
  void SetDescriptorHeap( int index, DescriptorHeap& heap, int offset ) override;
  void SetConstantValues( int index, const void* values, int numValues, int offset ) override;
  void SetRayTracingScene( int index, RTTopLevelAccelerator& accelerator ) override;
  void SetPrimitiveType( PrimitiveType primitiveType ) override;

  void Draw( int vertexCount, int instanceCount = 1, int startVertex = 0, int startInstance = 0 ) override;
  void DrawIndexed( int indexCount, int instanceCount, int startIndex, int baseVertex, int startInstance ) override;

  void SetComputeConstantValues( int index, const void* values, int numValues, int offset ) override;
  void SetComputeConstantBuffer( int index, Resource& resource ) override;
  void SetComputeShaderResourceView( int index, Resource& resource ) override;
  void SetComputeUnorderedAccessView( int index, Resource& resource ) override;
  void SetComputeRayTracingScene( int index, RTTopLevelAccelerator& accelerator ) override;
  void SetComputeDescriptorHeap( int index, DescriptorHeap& heap, int offset ) override;

  void SetVariableRateShading( VRSBlock block ) override;

  void Dispatch( int groupsX, int groupsY, int groupsZ ) override;
  void DispatchRays( int width, int height, int depth ) override;

  void ExecuteIndirect( CommandSignature& commandSignature, Resource& argsBuffer, int argsOffset, Resource& countBuffer, int countOffset, int maximumCount ) override;

  void AddUAVBarrier( eastl::initializer_list< eastl::reference_wrapper< Resource > > resources ) override;
  void AddNativeUAVBarrier( eastl::initializer_list< void* > resources ) override;

  void GenerateMipmaps( Resource& resource ) override;

  void DearImGuiRender() override;

  void UploadTextureResource( eastl::unique_ptr< Resource > source, Resource& destination, const void* data, int stride, int rows ) override;
  void UploadTextureRegion( eastl::unique_ptr< Resource > source, Resource& destination, int mip, int left, int top, int width, int height ) override;
  void UploadBufferResource( eastl::unique_ptr< Resource > source, Resource& destination, const void* data, int dataSize ) override;

  void UpdateBufferRegion( eastl::unique_ptr< Resource > source, Resource& destination, int offset ) override;

  void CopyResource( Resource& source, Resource& destination ) override;

  void ResolveMSAA( Resource& source, Resource& destination ) override;

  void HoldResource( eastl::unique_ptr< Resource > resource ) override;
  void HoldResource( eastl::unique_ptr< RTTopLevelAccelerator > resource ) override;
  void HoldResource( IUnknown* unknown ) override;

  eastl::vector< eastl::unique_ptr< Resource > > TakeHeldResources() override;
  eastl::vector< eastl::unique_ptr< RTTopLevelAccelerator > > TakeHeldTLAS() override;
  eastl::vector< CComPtr< IUnknown > > TakeHeldUnknowns() override;

  void BeginEvent( const wchar_t* format, ... ) override;
  void EndEvent() override;

  void RegisterEndFrameCallback( EndFrameCallback&& callback ) override;
  eastl::vector< EndFrameCallback > TakeEndFrameCallbacks() override;

  void HoldResource( D3D12MA::Allocation* allocation );
  void HoldResource( AllocatedResource&& allocation );

  ID3D12GraphicsCommandList6* GetD3DGraphicsCommandList();

  uint64_t GetFrequency() const;

private:
  D3DCommandList( D3DDevice& device, D3DCommandAllocator& commandAllocator, CommandQueueType queueType, uint64_t queueFrequency );

  CComPtr< ID3D12GraphicsCommandList6 > d3dGraphicsCommandList;

  eastl::vector< eastl::unique_ptr< Resource > > heldResources;
  eastl::vector< eastl::unique_ptr< RTTopLevelAccelerator > > heldTLAS;
  eastl::vector< CComPtr< IUnknown > > heldUnknowns;
  eastl::vector< EndFrameCallback > endFrameCallbacks;

  uint64_t frequency = 1;

  D3D12_DISPATCH_RAYS_DESC rayDesc;

  D3DDevice& device;
};