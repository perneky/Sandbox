#pragma once

#include "../Device.h"
#include "AllocatedResource.h"

struct TileHeap;

class D3DDescriptorHeap;
class D3DCommandList;
class D3DCommandQueue;
class D3DComputeShader;
class D3DResource;
class D3DMemoryHeap;

namespace D3D12MA { class Allocator; }

class D3DDevice : public Device
{
  friend class D3DAdapter;
  friend struct D3DDeviceHelper;

public:
  ~D3DDevice();

  int GetMaxSampleCountForTextures( PixelFormat format ) const override;
  int GetMatchingSampleCountForTextures( PixelFormat format, int count ) const override;
  int GetNumberOfQualityLevelsForTextures( PixelFormat format, int samples ) const override;

  eastl::unique_ptr< CommandQueue >             CreateCommandQueue( CommandQueueType type ) override;
  eastl::unique_ptr< CommandAllocator >         CreateCommandAllocator( CommandQueueType type ) override;
  eastl::unique_ptr< CommandList >              CreateCommandList( CommandAllocator& commandAllocator, CommandQueueType queueType, uint64_t queueFrequency ) override;
  eastl::unique_ptr< PipelineState >            CreatePipelineState( PipelineDesc& desc, const wchar_t* debugName ) override;
  eastl::unique_ptr< CommandSignature >         CreateCommandSignature( CommandSignatureDesc& desc, PipelineState& pipelineState ) override;
  eastl::unique_ptr< Resource >                 CreateBuffer( ResourceType resourceType, HeapType heapType, bool unorderedAccess, int size, int elementSize, const wchar_t* debugName ) override;
  eastl::unique_ptr< RTBottomLevelAccelerator > CreateRTBottomLevelAccelerator( CommandList& commandList, Resource& vertexBuffer, int vertexCount, int positionElementSize, int vertexStride, Resource& indexBuffer, int indexSize, int indexCount, int infoIndex, bool opaque, bool allowUpdate, bool fastBuild ) override;
  eastl::unique_ptr< RTTopLevelAccelerator >    CreateRTTopLevelAccelerator( CommandList& commandList, eastl::vector< RTInstance > instances, int slot ) override;
  eastl::unique_ptr< Resource >                 CreateVolumeTexture( CommandList* commandList, int width, int height, int depth, const void* data, int dataSize, PixelFormat format, int slot, eastl::optional< int > uavSlot, const wchar_t* debugName ) override;
  eastl::unique_ptr< Resource >                 Create2DTexture( CommandList* commandList, int width, int height, const void* data, int dataSize, PixelFormat format, int samples, int sampleQuality, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName ) override;
  eastl::unique_ptr< Resource >                 CreateCubeTexture( CommandList* commandList, int width, const void* data, int dataSize, PixelFormat format, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName ) override;
  eastl::unique_ptr< Resource >                 CreateReserved2DTexture( int width, int height, PixelFormat format, int slot, bool mipLevels, const wchar_t* debugName ) override;
  eastl::unique_ptr< ComputeShader >            CreateComputeShader( const void* shaderData, int shaderSize, const wchar_t* debugName ) override;
  eastl::unique_ptr< MemoryHeap >               CreateMemoryHeap( uint64_t size, const wchar_t* debugName ) override;
  eastl::unique_ptr< GPUTimeQuery >             CreateGPUTimeQuery() override;

  void PreallocateTiles( CommandQueue& directQueue ) override;

  eastl::unique_ptr< RTShaders > CreateRTShaders( CommandList& commandList
                                              , const eastl::vector< uint8_t >& rootSignatureShaderBinary
                                              , const eastl::vector< uint8_t >& shaderBinary
                                              , const wchar_t* rayGenEntryName
                                              , const wchar_t* missEntryName
                                              , const wchar_t* anyHitEntryName
                                              , const wchar_t* closestHitEntryName
                                              , int attributeSize
                                              , int payloadSize
                                              , int maxRecursionDepth ) override;

  eastl::unique_ptr< Resource > Load2DTexture( CommandList& commandList, eastl::vector< uint8_t >&& textureData, int slot, const wchar_t* debugName ) override;
  eastl::unique_ptr< Resource > LoadCubeTexture( CommandList& commandList, eastl::vector< uint8_t >&& textureData, int slot, const wchar_t* debugName ) override;

  eastl::unique_ptr< Resource > Stream2DTexture( CommandQueue& directQueue
                                               , CommandList& commandList
                                               , const TFFHeader& tffHeader
                                               , eastl::unique_ptr< FileLoaderFile >&& fileHandle
                                               , int slot
                                               , const wchar_t* debugName ) override;

  eastl::unique_ptr< Resource > AllocateUploadBuffer( int dataSize, const wchar_t* resourceName = nullptr ) override;

  AllocatedResource AllocateResource( HeapType heapType, const D3D12_RESOURCE_DESC& desc, ResourceState resourceState, const D3D12_CLEAR_VALUE* optimizedClearValue = nullptr, bool committed = false );

  DescriptorHeap& GetShaderResourceHeap() override;
  DescriptorHeap& GetSamplerHeap() override;

  int GetUploadSizeForResource( Resource& resource ) override;

  void SetTextureLODBias( float bias ) override;

  void StartNewFrame() override;

  void CaptureNextFrames( int count ) override;

  void  DearImGuiNewFrame() override;
  void* GetDearImGuiHeap() override;

  eastl::wstring GetMemoryInfo( bool includeIndividualAllocations ) override;

  ID3D12Resource* RequestD3DRTScartchBuffer( D3DCommandList& commandList, int size );

  ID3D12DeviceX*        GetD3DDevice();
  ID3D12DescriptorHeap* GetD3DCPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type );
  ID3D12DescriptorHeap* GetD3DGPUDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE type );

  ID3D12DescriptorHeap* GetD3DDearImGuiHeap();

  ID3D12RootSignature*  GetMipMapGenD3DRootSignature();
  ID3D12PipelineState*  GetMipMapGenD3DPipelineState();
  ID3D12DescriptorHeap* GetMipMapGenD3DDescriptorHeap();
  int                   GetMipMapGenDescCounter();

private:
  D3DDevice( D3DAdapter& adapter );

  void UpdateSamplers();

  CComPtr< ID3D12Resource > CreateTileUploadBuffer( ID3D12Resource* targetTexture );

  eastl::unique_ptr< D3DResource > CreateTexture( CommandList* commandList, int width, int height, int depth, int slices, PixelFormat format, int samples, int sampleQuality, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName, bool reserved );

  CComPtr< ID3D12DeviceX > d3dDevice;

  eastl::unique_ptr< D3DDescriptorHeap > descriptorHeaps[ D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES ];

  CComPtr< ID3D12DescriptorHeap > d3dFeedbackHeap;

  AllocatedResource d3dRTScartchBuffer;
  int               d3dRTScratchBufferSize = 0;

  CComPtr< ID3D12DescriptorHeap > d3dDearImGuiHeap;

  eastl::unique_ptr< D3DComputeShader > mipmapGenComputeShader;
  CComPtr< ID3D12DescriptorHeap >       d3dmipmapGenHeap;
  int                                   mipmapGenDescCounter = 0;

  eastl::vector_map< PixelFormat, eastl::array< int, maxTextureSampleCount + 1 > > msQualities;

  float textureLODBias = 0;

  D3D12MA::Allocator* allocator = nullptr;

  eastl::vector_map< PixelFormat, eastl::unique_ptr< TileHeap > > tileHeaps;
};
