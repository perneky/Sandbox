#pragma once

#include "Types.h"

struct CommandQueue;
struct CommandAllocator;
struct CommandList;
struct PipelineState;
struct CommandSignature;
struct Resource;
struct RTBottomLevelAccelerator;
struct RTTopLevelAccelerator;
struct RTShaders;
struct DescriptorHeap;
struct MemoryHeap;
struct ComputeShader;
struct GPUTimeQuery;
struct TFFHeader;
struct FileLoaderFile;

struct Device
{
  static constexpr int maxTextureSampleCount = 32;

  static void EnableDebugExtensions();

  virtual ~Device() = default;

  virtual int GetMaxSampleCountForTextures( PixelFormat format ) const = 0;
  virtual int GetMatchingSampleCountForTextures( PixelFormat format, int count ) const = 0;
  virtual int GetNumberOfQualityLevelsForTextures( PixelFormat format, int samples ) const = 0;

  virtual eastl::unique_ptr< CommandQueue >             CreateCommandQueue( CommandQueueType type ) = 0;
  virtual eastl::unique_ptr< CommandAllocator >         CreateCommandAllocator( CommandQueueType type ) = 0;
  virtual eastl::unique_ptr< CommandList >              CreateCommandList( CommandAllocator& commandAllocator, CommandQueueType queueType, uint64_t queueFrequency ) = 0;
  virtual eastl::unique_ptr< PipelineState >            CreatePipelineState( PipelineDesc& desc, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< CommandSignature >         CreateCommandSignature( CommandSignatureDesc& desc, PipelineState& pipelineState ) = 0;
  virtual eastl::unique_ptr< Resource >                 CreateBuffer( ResourceType resourceType, HeapType heapType, bool unorderedAccess, int size, int elementSize, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< RTBottomLevelAccelerator > CreateRTBottomLevelAccelerator( CommandList& commandList, Resource& vertexBuffer, int vertexCount, int positionElementSize, int vertexStride, Resource& indexBuffer, int indexSize, int indexCount, int infoIndex, bool opaque, bool allowUpdate, bool fastBuild ) = 0;
  virtual eastl::unique_ptr< RTTopLevelAccelerator >    CreateRTTopLevelAccelerator( CommandList& commandList, eastl::vector< RTInstance > instances, int slot ) = 0;
  virtual eastl::unique_ptr< Resource >                 CreateVolumeTexture( CommandList* commandList, int width, int height, int depth, const void* data, int dataSize, PixelFormat format, int slot, eastl::optional< int > uavSlot, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< Resource >                 Create2DTexture( CommandList* commandList, int width, int height, const void* data, int dataSize, PixelFormat format, int samples, int sampleQuality, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< Resource >                 CreateCubeTexture( CommandList* commandList, int width, const void* data, int dataSize, PixelFormat format, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< Resource >                 CreateReserved2DTexture( int width, int height, PixelFormat format, int slot, bool mipLevels, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< ComputeShader >            CreateComputeShader( const void* shaderData, int shaderSize, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< MemoryHeap >               CreateMemoryHeap( uint64_t size, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< GPUTimeQuery >             CreateGPUTimeQuery() = 0;

  virtual void PreallocateTiles( CommandQueue& directQueue ) = 0;

  virtual eastl::unique_ptr< RTShaders > CreateRTShaders( CommandList& commandList
                                                      , const eastl::vector< uint8_t >& rootSignatureShaderBinary
                                                      , const eastl::vector< uint8_t >& shaderBinary
                                                      , const wchar_t* rayGenEntryName
                                                      , const wchar_t* missEntryName
                                                      , const wchar_t* anyHitEntryName
                                                      , const wchar_t* closestHitEntryName
                                                      , int attributeSize
                                                      , int payloadSize
                                                      , int maxRecursionDepth ) = 0;


  virtual eastl::unique_ptr< Resource > Load2DTexture( CommandList& commandList, eastl::vector< uint8_t >&& textureData, int slot, const wchar_t* debugName ) = 0;
  virtual eastl::unique_ptr< Resource > LoadCubeTexture( CommandList& commandList, eastl::vector< uint8_t >&& textureData, int slot, const wchar_t* debugName ) = 0;

  virtual eastl::unique_ptr< Resource > Stream2DTexture( CommandQueue& directQueue
                                                       , CommandList& commandList
                                                       , const TFFHeader& tffHeader
                                                       , eastl::unique_ptr< FileLoaderFile >&& fileHandle
                                                       , int slot
                                                       , const wchar_t* debugName ) = 0;

  virtual eastl::unique_ptr< Resource > AllocateUploadBuffer( int dataSize, const wchar_t* resourceName = nullptr ) = 0;

  virtual DescriptorHeap& GetShaderResourceHeap() = 0;
  virtual DescriptorHeap& GetSamplerHeap() = 0;

  virtual int GetUploadSizeForResource( Resource& resource ) = 0;

  virtual void SetTextureLODBias( float bias ) = 0;

  virtual void StartNewFrame() = 0;

  virtual void CaptureNextFrames( int count ) = 0;

  virtual void  DearImGuiNewFrame() = 0;
  virtual void* GetDearImGuiHeap() = 0;

  virtual eastl::wstring GetMemoryInfo( bool includeIndividualAllocations ) = 0;

  eastl::unique_ptr< Resource > Create2DTexture( CommandList* commandList, int width, int height, const void* data, int dataSize, PixelFormat format, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName );
};
