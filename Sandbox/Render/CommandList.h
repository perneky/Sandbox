#pragma once

#include "Types.h"

struct Resource;
struct PipelineState;
struct Color;
struct RTTopLevelAccelerator;
struct DescriptorHeap;
struct ComputeShader;
struct ResourceDescriptor;
struct CommandSignature;
struct RTShaders;

enum class VRSBlock : uint32_t;

struct CommandList
{
  struct ResourceStateChange
  {
    Resource& resource;
    ResourceState newState;
  };

  virtual ~CommandList() = default;

  virtual void BindHeaps() = 0;

  virtual void ChangeResourceState( Resource& resource, ResourceState newState ) = 0;
  virtual void ChangeResourceState( eastl::initializer_list< ResourceStateChange > resources ) = 0;

  virtual void ClearRenderTarget( Resource& texture, const Color& color ) = 0;
  virtual void ClearDepthStencil( Resource& texture, float depth ) = 0;
  virtual void ClearUnorderedAccess( Resource& resource, uint32_t values[ 4 ] ) = 0;

  virtual void SetPipelineState( PipelineState& pipelineState ) = 0;
  virtual void SetComputeShader( ComputeShader& shader ) = 0;
  virtual void SetRayTracingShader( RTShaders& shaders ) = 0;

  virtual void SetRenderTarget( Resource& colorTexture, Resource* depthTexture ) = 0;
  virtual void SetRenderTarget( const eastl::vector< Resource* >& colorTextures, Resource* depthTexture ) = 0;
  virtual void SetRenderTarget( ResourceDescriptor& colorTextureDesciptor, ResourceDescriptor* depthTextureDesciptor ) = 0;
  virtual void SetRenderTarget( const eastl::vector< ResourceDescriptor* >& colorTextureDesciptors, ResourceDescriptor* depthTextureDesciptor ) = 0;
  virtual void SetViewport( int left, int top, int width, int height ) = 0;
  virtual void SetScissor( int left, int top, int width, int height ) = 0;
  virtual void SetVertexBuffer( Resource& resource ) = 0;
  virtual void SetIndexBuffer( Resource& resource ) = 0;
  virtual void SetVertexBufferToNull() = 0;
  virtual void SetIndexBufferToNull() = 0;
  virtual void SetConstantBuffer( int index, Resource& resource ) = 0;
  virtual void SetShaderResourceView( int index, Resource& resource ) = 0;
  virtual void SetUnorderedAccessView( int index, Resource& resource ) = 0;
  virtual void SetDescriptorHeap( int index, DescriptorHeap& heap, int offset ) = 0;
  virtual void SetConstantValues( int index, const void* values, int numValues, int offset = 0 ) = 0;
  virtual void SetRayTracingScene( int index, RTTopLevelAccelerator& accelerator ) = 0;
  virtual void SetPrimitiveType( PrimitiveType primitiveType ) = 0;

  virtual void Draw( int vertexCount, int instanceCount = 1, int startVertex = 0, int startInstance = 0 ) = 0;
  virtual void DrawIndexed( int indexCount, int instanceCount = 1, int startIndex = 0, int baseVertex = 0, int startInstance = 0 ) = 0;
  
  virtual void SetComputeConstantValues( int index, const void* values, int numValues, int offset = 0 ) = 0;
  virtual void SetComputeConstantBuffer( int index, Resource& resource ) = 0;
  virtual void SetComputeShaderResourceView( int index, Resource& resource ) = 0;
  virtual void SetComputeUnorderedAccessView( int index, Resource& resource ) = 0;
  virtual void SetComputeRayTracingScene( int index, RTTopLevelAccelerator& accelerator ) = 0;
  virtual void SetComputeDescriptorHeap( int index, DescriptorHeap& heap, int offset ) = 0;

  virtual void SetVariableRateShading( VRSBlock block ) = 0;

  virtual void Dispatch( int groupsX, int groupsY, int groupsZ ) = 0;
  virtual void DispatchRays( int width, int height, int depth ) = 0;

  virtual void ExecuteIndirect( CommandSignature& commandSignature, Resource& argsBuffer, int argsOffset, Resource& countBuffer, int countOffset, int maximumCount ) = 0;

  virtual void GenerateMipmaps( Resource& resource ) = 0;

  virtual void AddUAVBarrier( eastl::initializer_list< eastl::reference_wrapper< Resource > > resources ) = 0;
  virtual void AddNativeUAVBarrier( eastl::initializer_list< void* > resources ) = 0;

  virtual void DearImGuiRender() = 0;

  virtual void UploadTextureResource( eastl::unique_ptr< Resource > source, Resource& destination, const void* data, int stride, int rows ) = 0;
  virtual void UploadTextureRegion( eastl::unique_ptr< Resource > source, Resource& destination, int mip, int left, int top, int width, int height ) = 0;
  virtual void UploadBufferResource( eastl::unique_ptr< Resource > source, Resource& destination, const void* data, int dataSize ) = 0;

  virtual void UpdateBufferRegion( eastl::unique_ptr< Resource > source, Resource& destination, int offset ) = 0;

  virtual void CopyResource( Resource& source, Resource& destination ) = 0;

  virtual void ResolveMSAA( Resource& source, Resource& destination ) = 0;

  virtual void HoldResource( eastl::unique_ptr< Resource > resource ) = 0;
  virtual void HoldResource( eastl::unique_ptr< RTTopLevelAccelerator > resource ) = 0;
  virtual void HoldResource( IUnknown* unknown ) = 0;

  virtual eastl::vector< eastl::unique_ptr< Resource > > TakeHeldResources() = 0;
  virtual eastl::vector< eastl::unique_ptr< RTTopLevelAccelerator > > TakeHeldTLAS() = 0;
  virtual eastl::vector< CComPtr< IUnknown > > TakeHeldUnknowns() = 0;

  virtual void BeginEvent( const wchar_t* format, ... ) = 0;
  virtual void EndEvent() = 0;

  using EndFrameCallback = eastl::function< void() >;
  virtual void RegisterEndFrameCallback( EndFrameCallback&& callback ) = 0;
  virtual eastl::vector< EndFrameCallback > TakeEndFrameCallbacks() = 0;

  template< typename T >
  void SetConstantValues( int index, const T& values, int offset )
  {
    static_assert( sizeof( values ) % 4 == 0, "Passed value should be 4 byte values only!" );
    SetConstantValues( index, &values, sizeof( values ) / 4, offset );
  }

  template< typename T >
  void SetComputeConstantValues( int index, const T& values, int offset )
  {
    static_assert( sizeof( values ) % 4 == 0, "Passed value should be 4 byte values only!" );
    SetComputeConstantValues( index, &values, sizeof( values ) / 4, offset );
  }
};

struct GPUSection
{
  GPUSection( CommandList& commandList, const wchar_t* name )
    : commandList( commandList )
  {
    commandList.BeginEvent( name );
  }
  ~GPUSection()
  {
    Close();
  }
  void Close()
  {
    if ( !closed )
      commandList.EndEvent();
    closed = true;
  }

private:
  CommandList& commandList;
  bool closed = false;
};