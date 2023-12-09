#pragma once

#include "Types.h"
#include "ShaderStructures.h"
#include "TextureStreamers/TextureStreamer.h"

struct Window;
struct Factory;
struct Adapter;
struct Device;
struct Swapchain;
struct Resource;
struct CommandList;
struct CommandAllocator;
struct PipelineState;
struct DescriptorHeap;
struct ComputeShader;
struct CommandSignature;
struct RTTopLevelAccelerator;

class CommandQueueManager;
class AnimationSet;

class RenderManager
{
public:
  static constexpr PixelFormat SDRFormat          = PixelFormat::RGBA1010102UN;
  static constexpr PixelFormat HDRFormat          = PixelFormat::RGBA16161616F;
  static constexpr PixelFormat HDRFormat2         = PixelFormat::RG1616F;
  static constexpr PixelFormat DepthFormat        = PixelFormat::D32;
  static constexpr PixelFormat ShadowFormat       = PixelFormat::RG1616F;
  static constexpr PixelFormat ShadowTransFormat  = PixelFormat::RGBA8888UN;
  static constexpr PixelFormat ReflDepthFormat    = PixelFormat::R32F;
  static constexpr PixelFormat AOFormat           = PixelFormat::R8UN;
  static constexpr PixelFormat GIFormat           = PixelFormat::RGBA16161616F;
  static constexpr PixelFormat MotionVectorFormat = PixelFormat::RG1616F;
  static constexpr PixelFormat LumaFormat         = PixelFormat::R8U;
  static constexpr PixelFormat HQSFormat          = PixelFormat::R8UN;
  static constexpr PixelFormat TextureMipFormat   = PixelFormat::R16F;
  static constexpr PixelFormat GeometryIdsFormat  = PixelFormat::RG1616U;

  static bool           CreateInstance( eastl::shared_ptr< Window > window );
  static RenderManager& GetInstance();
  static void           DeleteInstance();

  void UpdateBeforeFrame( CommandList& commandList );
  TextureStreamer::UpdateResult UpdateAfterFrame( CommandList& commandList, CommandQueueType commandQueueType, uint64_t fence );

  void RenderDebugTexture( CommandList& commandList, int texIndex, int screenWidth, int screenHeight, DebugOutput debugOutput );

  void IdleGPU();

  void SetUp( CommandList& commandList, const eastl::wstring& textureFolder );

  CommandAllocator* RequestCommandAllocator( CommandQueueType queueType );
  void DiscardCommandAllocator( CommandQueueType queueType, CommandAllocator* allocator, uint64_t fenceValue );
  eastl::unique_ptr< CommandList > CreateCommandList( CommandAllocator& allocator, CommandQueueType queueType );

  void PrepareNextSwapchainBuffer( uint64_t fenceValue );
  uint64_t Submit( eastl::unique_ptr< CommandList >&& commandList, CommandQueueType queueType, bool wait );
  uint64_t Submit( eastl::vector< eastl::unique_ptr< CommandList > >&& commandLists, CommandQueueType queueType, bool wait );
  uint64_t Present( uint64_t fenceValue, bool useVSync );

  Device&           GetDevice();
  Swapchain&        GetSwapchain();
  PipelineState&    GetPipelinePreset( PipelinePresets preset );
  CommandSignature& GetCommandSignature( CommandSignatures preset );
  DescriptorHeap&   GetShaderResourceHeap();
  DescriptorHeap&   GetSamplerHeap();
  CommandQueue&     GetCommandQueue( CommandQueueType type );

  Resource* GetGlobalTextureFeedbackBuffer( CommandList& commandList );

  void TidyUp();

  eastl::unique_ptr< Resource > GetUploadBufferForSize( int size );
  eastl::unique_ptr< Resource > GetUploadBufferForResource( Resource& resource );
  void ReuseUploadConstantBuffer( eastl::unique_ptr< Resource > buffer );

  void RecreateWindowSizeDependantResources( CommandList& commandList );

  int GetMSAASamples() const;
  int GetMSAAQuality() const;

  int Get2DTexture( CommandQueueType commandQueueType, CommandList& commandList, const eastl::wstring& path, int* refTexutreId = nullptr );

  int Get2DTextureCount() const;

  TextureStreamer::MemoryStats GetMemoryStats() const;

private:
  RenderManager( eastl::shared_ptr< Window > window );
  ~RenderManager();

  static RenderManager* instance;

  int msaaSamples;
  int msaaQuality;

  eastl::shared_ptr< Window > window;

  eastl::unique_ptr< Factory >   factory;
  eastl::unique_ptr< Adapter >   adapter;
  eastl::unique_ptr< Device >    device;
  eastl::unique_ptr< Swapchain > swapchain;

  eastl::unique_ptr< CommandQueueManager > commandQueueManager;

  eastl::unique_ptr< PipelineState > pipelinePresets[ int( PipelinePresets::PresetCount ) ];
  eastl::unique_ptr< CommandSignature > commandSignatures[ int( CommandSignatures::PresetCount ) ];

  eastl::vector_map< CommandQueueType, eastl::map< uint64_t, eastl::vector< eastl::unique_ptr< Resource > > > >              stagingResources;
  eastl::vector_map< CommandQueueType, eastl::map< uint64_t, eastl::vector< eastl::unique_ptr< RTTopLevelAccelerator > > > > stagingTLAS;
  eastl::vector_map< CommandQueueType, eastl::map< uint64_t, eastl::vector< CComPtr< IUnknown > > > >                        stagingUnknowns;
  eastl::vector_map< CommandQueueType, eastl::map< uint64_t, eastl::vector< eastl::function< void() > > > >                  endFrameCallbacks;

  eastl::unique_ptr< Resource > randomTexture;
  eastl::unique_ptr< Resource > globalTextureFeedbackBuffer;
  eastl::unique_ptr< Resource > globalTextureFeedbackReadbackBuffer;

  eastl::unique_ptr< TextureStreamer > textureStreamer;

  eastl::mutex                                                      uploadBuffersLock;
  eastl::map< int, eastl::vector< eastl::unique_ptr< Resource > > > uploadBuffers;

  uint64_t pendingGlobalTextureReadbackFence = 0;
};
