#include "RenderManager.h"
#include "Factory.h"
#include "Adapter.h"
#include "Device.h"
#include "Swapchain.h"
#include "PipelineState.h"
#include "CommandSignature.h"
#include "Resource.h"
#include "ResourceDescriptor.h"
#include "DescriptorHeap.h"
#include "Mesh.h"
#include "RTTopLevelAccelerator.h"
#include "ShaderStructures.h"
#include "ShaderValues.h"
#include "CommandQueueManager.h"
#include "CommandQueue.h"
#include "CommandAllocatorPool.h"
#include "CommandList.h"
#include "ComputeShader.h"
#include "Common/Files.h"
#include "Platform/Window.h"

#include "TextureStreamers/TextureStreamer_Immediate.h"
#include "TextureStreamers/TextureStreamer_Tiled.h"

RenderManager* RenderManager::instance = nullptr;

bool RenderManager::CreateInstance( eastl::shared_ptr< Window > window )
{
  instance = new RenderManager( window );
  return true;
}

RenderManager& RenderManager::GetInstance()
{
  assert( instance );
  return *instance;
}

void RenderManager::DeleteInstance()
{
  if ( instance )
    delete instance;
  instance = nullptr;
}

void RenderManager::UpdateBeforeFrame( CommandList& commandList )
{
  textureStreamer->UpdateBeforeFrame( *device, commandList );
}

TextureStreamer::UpdateResult RenderManager::UpdateAfterFrame( CommandList& commandList, CommandQueueType commandQueueType, uint64_t fence )
{
  #if ENABLE_TEXTURE_STREAMING
    if ( pendingGlobalTextureReadbackFence )
    {
      if ( commandQueueManager->GetQueue( CommandQueueType::Direct ).IsFenceComplete( pendingGlobalTextureReadbackFence ) )
      {
        uint32_t* feedbackData = (uint32_t*)globalTextureFeedbackReadbackBuffer->Map();

        auto streamingCommandLists = textureStreamer->UpdateAfterFrame( *device, commandQueueManager->GetQueue( commandQueueType ), commandList, fence, feedbackData );

        globalTextureFeedbackReadbackBuffer->Unmap();

        pendingGlobalTextureReadbackFence = 0;

        return streamingCommandLists;
      }
      else
        return {};
    }
    else
    {
      uint32_t clear[] = { 255, 255, 255, 255 };

      commandList.CopyResource( *globalTextureFeedbackBuffer, *globalTextureFeedbackReadbackBuffer );
      commandList.ClearUnorderedAccess( *globalTextureFeedbackBuffer, clear );
      pendingGlobalTextureReadbackFence = fence;
      return {};
    }
  #else
    return {};
  #endif
}

void RenderManager::RenderDebugTexture( CommandList& commandList, int texIndex, int screenWidth, int screenHeight )
{
  struct
  {
    XMFLOAT2 leftTop;
    XMFLOAT2 widthHeight;
    uint32_t mipLevel;
    uint32_t sceneTextureId;
  } quadParams;

  auto texture = textureStreamer->GetTexture( texIndex );
  if ( !texture )
    return;

  int   mipLevels = texture->GetTextureMipLevels();
  float aspect    = float( texture->GetTextureWidth() ) / texture->GetTextureHeight();

  int border = screenHeight / 100;

  RECT area = {};

  area.left   = border;
  area.top    = border;
  area.right  = screenWidth / 3 - border;
  area.bottom = int( area.top + ( area.right - area.left ) / aspect );

  for ( int mip = 0; mip < mipLevels; ++mip )
  {
    quadParams.leftTop.x      =    ( float( area.left   ) / screenWidth  ) * 2 - 1;
    quadParams.leftTop.y      = -( ( float( area.top    ) / screenHeight ) * 2 - 1 );
    quadParams.widthHeight.x  =    ( float( area.right - area.left ) / screenWidth  ) * 2;
    quadParams.widthHeight.y  = -( ( float( area.bottom - area.top ) / screenHeight ) * 2 );
    quadParams.mipLevel       = mip;
    quadParams.sceneTextureId = texIndex;

    commandList.SetPipelineState( RenderManager::GetInstance().GetPipelinePreset( PipelinePresets::QuadDebug ) );
    commandList.SetVertexBufferToNull();
    commandList.SetIndexBufferToNull();
    commandList.SetPrimitiveType( PrimitiveType::TriangleStrip );
    commandList.SetConstantValues( 0, quadParams, 0 );
    commandList.SetDescriptorHeap( 2, RenderManager::GetInstance().GetShaderResourceHeap(), Scene2DResourceBaseSlot );
    commandList.SetDescriptorHeap( 3, RenderManager::GetInstance().GetShaderResourceHeap(), Scene2DMipTailBaseSlot );
    commandList.SetDescriptorHeap( 4, RenderManager::GetInstance().GetShaderResourceHeap(), Engine2DTileTexturesBaseSlot );
    commandList.Draw( 4 );

    RECT newArea = {};
    newArea.left   = area.right + border;
    newArea.top    = area.top;
    newArea.right  = newArea.left + eastl::max( ( area.right - area.left ) / 2, 1L );
    newArea.bottom = newArea.top  + eastl::max( ( area.bottom - area.top ) / 2, 1L );

    area = newArea;
  }
}

void RenderManager::IdleGPU()
{
  commandQueueManager->IdleGPU();
}

void RenderManager::SetUp( CommandList& commandList, const eastl::wstring& textureFolder )
{
  struct NoiseTexel
  {
    unsigned r : 10;
    unsigned g : 10;
    unsigned b : 10;
    unsigned a : 2;
  };

  srand( 68425 );
  constexpr int noiseSize = RandomTextureSize;
  std::vector< NoiseTexel > noiseTexels( noiseSize * noiseSize * noiseSize );
  for ( auto& t : noiseTexels )
  {
    t.r = rand() % ( 1 << 10 );
    t.g = rand() % ( 1 << 10 );
    t.b = rand() % ( 1 << 10 );
    t.a = 4;
  }

  randomTexture = device->CreateVolumeTexture( commandList, noiseSize, noiseSize, noiseSize, noiseTexels.data(), int( noiseTexels.size() * 4 ), PixelFormat::RGBA1010102UN, RandomTextureSlot, eastl::nullopt, L"NoiseVolume" );

  #if ENABLE_TEXTURE_STREAMING
    textureStreamer = eastl::make_unique< TiledTextureStreamer >( *device );
  #else
    textureStreamer = eastl::make_unique< ImmediateTextureStreamer >();
  #endif
}

CommandAllocator* RenderManager::RequestCommandAllocator( CommandQueueType queueType )
{
  auto& queue          = commandQueueManager->GetQueue( queueType );
  auto& allocatorPool  = commandQueueManager->GetAllocatorPool( queueType );
  return allocatorPool.RequestAllocator( *device, queue.GetLastCompletedFenceValue() );
}

void RenderManager::DiscardCommandAllocator( CommandQueueType queueType, CommandAllocator* allocator, uint64_t fenceValue )
{
  commandQueueManager->GetAllocatorPool( queueType ).DiscardAllocator( fenceValue, allocator );
}

eastl::unique_ptr< CommandList > RenderManager::CreateCommandList( CommandAllocator& allocator, CommandQueueType queueType )
{
  auto commandList = device->CreateCommandList( allocator, queueType, commandQueueManager->GetQueue( queueType ).GetFrequency() );
  return commandList;
}

void RenderManager::PrepareNextSwapchainBuffer( uint64_t fenceValue )
{
  commandQueueManager->GetQueue( CommandQueueType::Direct ).WaitForFence( fenceValue );
}

uint64_t RenderManager::Submit( eastl::unique_ptr<CommandList>&& commandList, CommandQueueType queueType, bool wait )
{
  eastl::vector< eastl::unique_ptr< CommandList > > commandLists;
  commandLists.emplace_back( eastl::move( commandList ) );
  return Submit( eastl::move( commandLists ), queueType, wait );
}

uint64_t RenderManager::Submit( eastl::vector< eastl::unique_ptr< CommandList > >&& commandLists, CommandQueueType queueType, bool wait )
{
  auto& queue      = commandQueueManager->GetQueue( queueType );
  auto  fenceValue = queue.Submit( commandLists );
  if ( wait )
    queue.WaitForFence( fenceValue );
  else
  {
    for ( auto& commandList : commandLists )
    {
      auto heldResources = commandList->TakeHeldResources();
      if ( !heldResources.empty() )
      {
        auto& hold = stagingResources[ queueType ][ fenceValue ];
        eastl::move( heldResources.begin(), heldResources.end(), eastl::back_inserter( hold ) );
      }

      auto heldTLAS = commandList->TakeHeldTLAS();
      if ( !heldTLAS.empty() )
      {
        auto& hold = stagingTLAS[ queueType ][ fenceValue ];
        eastl::move( heldTLAS.begin(), heldTLAS.end(), eastl::back_inserter( hold ) );
      }

      auto heldUnknowns = commandList->TakeHeldUnknowns();
      if ( !heldUnknowns.empty() )
      {
        auto& hold = stagingUnknowns[ queueType ][ fenceValue ];
        eastl::move( heldUnknowns.begin(), heldUnknowns.end(), eastl::back_inserter( hold ) );
      }

      auto newEndFrameCallbacks = commandList->TakeEndFrameCallbacks();
      if ( !newEndFrameCallbacks.empty() )
      {
        auto& hold = endFrameCallbacks[ queueType ][ fenceValue ];
        eastl::move( newEndFrameCallbacks.begin(), newEndFrameCallbacks.end(), eastl::back_inserter( hold ) );
      }
    }
  }
  return fenceValue;
}

uint64_t RenderManager::Present( uint64_t fenceValue, bool useVSync )
{
  return swapchain->Present( fenceValue, useVSync );
}

PipelineState& RenderManager::GetPipelinePreset( PipelinePresets preset )
{
  return *pipelinePresets[ int( preset ) ];
}

CommandSignature& RenderManager::GetCommandSignature( CommandSignatures preset )
{
  return *commandSignatures[ int( preset ) ];
}

DescriptorHeap& RenderManager::GetShaderResourceHeap()
{
  return device->GetShaderResourceHeap();
}

DescriptorHeap& RenderManager::GetSamplerHeap()
{
  return device->GetSamplerHeap();
}

CommandQueue& RenderManager::GetCommandQueue( CommandQueueType type )
{
  return commandQueueManager->GetQueue( type );
}

Resource* RenderManager::GetGlobalTextureFeedbackBuffer( CommandList& commandList )
{
  #if ENABLE_TEXTURE_STREAMING
    if ( !globalTextureFeedbackBuffer )
    {
      auto texStats = textureStreamer->GetMemoryStats();
      globalTextureFeedbackBuffer = device->CreateBuffer( ResourceType::Buffer
                                                        , HeapType::Default
                                                        , true
                                                        , sizeof( uint32_t ) * texStats.textureCount
                                                        , sizeof( uint32_t )
                                                        , L"Global Texture Feedback");
      globalTextureFeedbackReadbackBuffer = device->CreateBuffer( ResourceType::Buffer
                                                                , HeapType::Readback
                                                                , false
                                                                , sizeof( uint32_t ) * texStats.textureCount
                                                                , sizeof( uint32_t )
                                                                , L"Global Texture Feedback Readback" );

      auto uav = device->GetShaderResourceHeap().RequestDescriptorAuto( *device, ResourceDescriptorType::UnorderedAccessView, 0, *globalTextureFeedbackBuffer, 0 );
      globalTextureFeedbackBuffer->AttachResourceDescriptor( ResourceDescriptorType::UnorderedAccessView, eastl::move( uav ) );

      commandList.ChangeResourceState( *globalTextureFeedbackBuffer, ResourceStateBits::UnorderedAccess );

      uint32_t clear[] = { 255, 255, 255, 255 };
      commandList.ClearUnorderedAccess( *globalTextureFeedbackBuffer, clear );
    }
  #endif // 0

  return globalTextureFeedbackBuffer.get();
}

void RenderManager::TidyUp()
{
  for ( int queueType = 0; queueType < 3; ++queueType )
  {
    auto& queue     = commandQueueManager->GetQueue( CommandQueueType( queueType ) );
    auto& resources = stagingResources[ CommandQueueType( queueType ) ];
    auto& tlas      = stagingTLAS[ CommandQueueType( queueType ) ];
    auto& unknowns  = stagingUnknowns[ CommandQueueType( queueType ) ];
    auto& efcs      = endFrameCallbacks[ CommandQueueType( queueType ) ];

    for ( auto iter = resources.begin(); iter != resources.end(); )
    {
      if ( queue.IsFenceComplete( iter->first ) )
      {
        for ( auto& resource : iter->second )
        {
          if ( resource->IsUploadResource() && resource->GetResourceType() == ResourceType::Buffer )
            ReuseUploadConstantBuffer( eastl::move( resource ) );
        }
        iter = resources.erase( iter );
      }
      else
        ++iter;
    }

    for ( auto iter = tlas.begin(); iter != tlas.end(); )
    {
      if ( queue.IsFenceComplete( iter->first ) )
        iter = tlas.erase( iter );
      else
        ++iter;
    }

    for ( auto iter = unknowns.begin(); iter != unknowns.end(); )
    {
      if ( queue.IsFenceComplete( iter->first ) )
        iter = unknowns.erase( iter );
      else
        ++iter;
    }

    for ( auto iter = efcs.begin(); iter != efcs.end(); )
    {
      if ( queue.IsFenceComplete( iter->first ) )
      {
        for ( auto& efc : iter->second )
          efc();
        iter = efcs.erase( iter );
      }
      else
        ++iter;
    }
  }
}

eastl::unique_ptr<Resource> RenderManager::GetUploadBufferForSize( int size )
{
  eastl::lock_guard< eastl::mutex > lock( uploadBuffersLock );

  auto iter = uploadBuffers.find( size );
  if ( iter == uploadBuffers.end() || iter->second.empty() )
    return device->CreateBuffer( ResourceType::Buffer, HeapType::Upload, false, size, 0, L"UploadCB" );
  else
  {
    auto buffer = eastl::move( iter->second.back() );
    iter->second.pop_back();
    return buffer;
  }
}

eastl::unique_ptr< Resource > RenderManager::GetUploadBufferForResource( Resource& resource )
{
  return GetUploadBufferForSize( device->GetUploadSizeForResource( resource ) );
}

void RenderManager::ReuseUploadConstantBuffer( eastl::unique_ptr< Resource > buffer )
{
  auto bufferSize = buffer->GetBufferSize();
  uploadBuffers[ bufferSize ].emplace_back( eastl::move( buffer ) );
}

Device& RenderManager::GetDevice()
{
  return *device;
}

Swapchain& RenderManager::GetSwapchain()
{
  return *swapchain;
}

RenderManager::RenderManager( eastl::shared_ptr< Window > window )
  : window( window )
{
  Device::EnableDebugExtensions();

  factory = Factory::Create();
  adapter = factory->CreateDefaultAdapter();
  device  = adapter->CreateDevice();

  msaaSamples = 1;// device->GetMaxSampleCountForTextures( SDRFormat );
  msaaQuality = 1;// device->GetNumberOfQualityLevelsForTextures( SDRFormat, msaaSamples );

  commandQueueManager.reset( new CommandQueueManager( *device ) );

  swapchain = factory->CreateSwapchain( *device, commandQueueManager->GetQueue( CommandQueueType::Direct ), *window );
  swapchain->BuildBackBufferTextures( *device );

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/ModelTranslucentVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/ModelTranslucentPS.cso" );

    PipelineDesc pipelineDesc;

    pipelineDesc.depthStencilDesc.depthWrite    = false;
    pipelineDesc.blendDesc                      = BlendDesc( BlendPreset::Blend );
    pipelineDesc.vsData                         = vertexShader.data();
    pipelineDesc.psData                         = pixelShader.data();
    pipelineDesc.vsSize                         = vertexShader.size();
    pipelineDesc.psSize                         = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ]              = RenderManager::HDRFormat;
    pipelineDesc.depthFormat                    = RenderManager::DepthFormat;
    pipelineDesc.samples                        = msaaSamples;
    pipelineDesc.sampleQuality                  = msaaQuality;
    pipelinePresets[ int( PipelinePresets::MeshTranslucent ) ] = device->CreatePipelineState( pipelineDesc, L"MeshTranslucent" );

    pipelineDesc.rasterizerDesc.cullMode = RasterizerDesc::CullMode::None;
    pipelinePresets[ int( PipelinePresets::MeshTranslucentTwoSided ) ] = device->CreatePipelineState( pipelineDesc, L"MeshTranslucentTwoSided" );
  }

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/ModelDepthVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/ModelDepthPS.cso" );

    PipelineDesc pipelineDesc;

    pipelineDesc.blendDesc         = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData            = vertexShader.data();
    pipelineDesc.psData            = pixelShader.data();
    pipelineDesc.vsSize            = vertexShader.size();
    pipelineDesc.psSize            = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ] = RenderManager::MotionVectorFormat;
    pipelineDesc.targetFormat[ 1 ] = RenderManager::TextureMipFormat;
    pipelineDesc.targetFormat[ 2 ] = RenderManager::GeometryIdsFormat;
    pipelineDesc.depthFormat       = RenderManager::DepthFormat;
    pipelineDesc.samples           = msaaSamples;
    pipelineDesc.sampleQuality     = msaaQuality;

    pipelinePresets[ int( PipelinePresets::MeshDepth ) ] = device->CreatePipelineState( pipelineDesc, L"MeshDepth" );

    pipelineDesc.rasterizerDesc.cullMode = RasterizerDesc::CullMode::None;
    pipelinePresets[ int( PipelinePresets::MeshDepthTwoSided ) ] = device->CreatePipelineState( pipelineDesc, L"MeshDepthTwoSided" );
  }


  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/ModelDepthVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/ModelDepthAtestPS.cso" );

    PipelineDesc pipelineDesc;

    pipelineDesc.blendDesc         = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData            = vertexShader.data();
    pipelineDesc.psData            = pixelShader.data();
    pipelineDesc.vsSize            = vertexShader.size();
    pipelineDesc.psSize            = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ] = RenderManager::MotionVectorFormat;
    pipelineDesc.targetFormat[ 1 ] = RenderManager::TextureMipFormat;
    pipelineDesc.targetFormat[ 2 ] = RenderManager::GeometryIdsFormat;
    pipelineDesc.depthFormat       = RenderManager::DepthFormat;
    pipelineDesc.samples           = msaaSamples;
    pipelineDesc.sampleQuality     = msaaQuality;

    pipelinePresets[ int( PipelinePresets::MeshDepthAlphaTest ) ] = device->CreatePipelineState( pipelineDesc, L"MeshDepthAlphaTest" );

    pipelineDesc.rasterizerDesc.cullMode = RasterizerDesc::CullMode::None;
    pipelinePresets[ int( PipelinePresets::MeshDepthTwoSidedAlphaTest ) ] = device->CreatePipelineState( pipelineDesc, L"MeshDepthTwoSidedAlphaTest" );
  }

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/SkyVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/SkyPS.cso" );

    PipelineDesc pipelineDesc;

    pipelineDesc.depthStencilDesc.depthWrite = false;
    pipelineDesc.rasterizerDesc.cullMode     = RasterizerDesc::CullMode::None;
    pipelineDesc.blendDesc                   = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData                      = vertexShader.data();
    pipelineDesc.psData                      = pixelShader.data();
    pipelineDesc.vsSize                      = vertexShader.size();
    pipelineDesc.psSize                      = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ]           = RenderManager::HDRFormat;
    pipelineDesc.depthFormat                 = RenderManager::DepthFormat;
    pipelineDesc.samples                     = msaaSamples;
    pipelineDesc.sampleQuality               = msaaQuality;
    pipelinePresets[ int( PipelinePresets::Sky ) ] = device->CreatePipelineState( pipelineDesc, L"Sky" );

    pipelineDesc.depthStencilDesc.depthTest  = false;
    pipelineDesc.depthStencilDesc.depthWrite = false;
    pipelineDesc.depthFormat                 = PixelFormat::Unknown;
    pipelineDesc.samples                     = 1;
    pipelineDesc.sampleQuality               = 0;
    pipelinePresets[ int( PipelinePresets::SkyCube ) ] = device->CreatePipelineState( pipelineDesc, L"SkyCube" );
  }

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/ToneMappingVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/ToneMappingPS.cso" );

    PipelineDesc pipelineDesc;

    pipelineDesc.depthStencilDesc.depthTest  = false;
    pipelineDesc.depthStencilDesc.depthWrite = false;
    pipelineDesc.rasterizerDesc.cullMode     = RasterizerDesc::CullMode::None;
    pipelineDesc.blendDesc                   = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData                      = vertexShader.data();
    pipelineDesc.psData                      = pixelShader.data();
    pipelineDesc.vsSize                      = vertexShader.size();
    pipelineDesc.psSize                      = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ]           = RenderManager::SDRFormat;
    pipelineDesc.depthFormat                 = PixelFormat::Unknown;
    pipelineDesc.samples                     = 1;
    pipelineDesc.sampleQuality               = 0;
    pipelinePresets[ int( PipelinePresets::ToneMapping ) ] = device->CreatePipelineState( pipelineDesc, L"ToneMapping" );
  }

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/DirectLightingVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/DirectLightingPS.cso" );

    PipelineDesc pipelineDesc;

    pipelineDesc.depthStencilDesc.depthTest  = false;
    pipelineDesc.depthStencilDesc.depthWrite = false;
    pipelineDesc.rasterizerDesc.cullMode     = RasterizerDesc::CullMode::None;
    pipelineDesc.blendDesc                   = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData                      = vertexShader.data();
    pipelineDesc.psData                      = pixelShader.data();
    pipelineDesc.vsSize                      = vertexShader.size();
    pipelineDesc.psSize                      = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ]           = RenderManager::HDRFormat;
    pipelineDesc.depthFormat                 = PixelFormat::Unknown;
    pipelineDesc.samples                     = 1;
    pipelineDesc.sampleQuality               = 0;
    pipelinePresets[ int( PipelinePresets::DirectLighting ) ] = device->CreatePipelineState( pipelineDesc, L"DirectLighting" );
  }

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/ScreenQuadVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/ScreenQuadPS.cso" );

    PipelineDesc pipelineDesc;
    pipelineDesc.rasterizerDesc.cullMode        = RasterizerDesc::CullMode::None;
    pipelineDesc.depthStencilDesc.depthFunction = DepthStencilDesc::Comparison::Always;
    pipelineDesc.depthStencilDesc.depthWrite    = false;
    pipelineDesc.blendDesc                      = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData                         = vertexShader.data();
    pipelineDesc.psData                         = pixelShader.data();
    pipelineDesc.vsSize                         = vertexShader.size();
    pipelineDesc.psSize                         = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ]              = RenderManager::SDRFormat;
    pipelineDesc.depthFormat                    = PixelFormat::Unknown;
    pipelineDesc.samples                        = 1;
    pipelineDesc.sampleQuality                  = 0;
    pipelinePresets[ int( PipelinePresets::QuadDebug ) ] = device->CreatePipelineState( pipelineDesc, L"QuadDebug" );
  }

  {
    auto vertexShader = ReadFileToMemory( L"Content/Shaders/DSTransferBufferDebugVS.cso" );
    auto  pixelShader = ReadFileToMemory( L"Content/Shaders/DSTransferBufferDebugPS.cso" );

    PipelineDesc pipelineDesc;
    pipelineDesc.rasterizerDesc.cullMode        = RasterizerDesc::CullMode::None;
    pipelineDesc.depthStencilDesc.depthFunction = DepthStencilDesc::Comparison::Always;
    pipelineDesc.depthStencilDesc.depthWrite    = false;
    pipelineDesc.blendDesc                      = BlendDesc( BlendPreset::DirectWrite );
    pipelineDesc.vsData                         = vertexShader.data();
    pipelineDesc.psData                         = pixelShader.data();
    pipelineDesc.vsSize                         = vertexShader.size();
    pipelineDesc.psSize                         = pixelShader.size();
    pipelineDesc.targetFormat[ 0 ]              = RenderManager::SDRFormat;
    pipelineDesc.depthFormat                    = PixelFormat::Unknown;
    pipelineDesc.samples                        = 1;
    pipelineDesc.sampleQuality                  = 0;
    pipelinePresets[ int( PipelinePresets::DSTransferBufferDebug ) ] = device->CreatePipelineState( pipelineDesc, L"DSTransferBufferDebug" );
  }

  {
    CommandSignatureDesc signatureDesc;

    signatureDesc.stride = sizeof( IndirectRender );
    signatureDesc.arguments.resize( 2 );
    signatureDesc.arguments[ 0 ].type                             = CommandSignatureDesc::Argument::Type::Constant;
    signatureDesc.arguments[ 0 ].constant.rootParameterIndex      = 0;
    signatureDesc.arguments[ 0 ].constant.num32BitValuesToSet     = sizeof( IndirectRender ) / 4 - 4;
    signatureDesc.arguments[ 0 ].constant.destOffsetIn32BitValues = 0;
    signatureDesc.arguments[ 1 ].type                             = CommandSignatureDesc::Argument::Type::Draw;

    commandSignatures[ int( CommandSignatures::MeshDepth                  ) ] = device->CreateCommandSignature( signatureDesc, GetPipelinePreset( PipelinePresets::MeshDepth ) );
    commandSignatures[ int( CommandSignatures::MeshDepthTwoSided          ) ] = device->CreateCommandSignature( signatureDesc, GetPipelinePreset( PipelinePresets::MeshDepthTwoSided ) );
    commandSignatures[ int( CommandSignatures::MeshDepthAlphaTest         ) ] = device->CreateCommandSignature( signatureDesc, GetPipelinePreset( PipelinePresets::MeshDepthAlphaTest ) );
    commandSignatures[ int( CommandSignatures::MeshDepthTwoSidedAlphaTest ) ] = device->CreateCommandSignature( signatureDesc, GetPipelinePreset( PipelinePresets::MeshDepthTwoSidedAlphaTest ) );
    commandSignatures[ int( CommandSignatures::MeshTranslucent            ) ] = device->CreateCommandSignature( signatureDesc, GetPipelinePreset( PipelinePresets::MeshTranslucent ) );
    commandSignatures[ int( CommandSignatures::MeshTranslucentTwoSided    ) ] = device->CreateCommandSignature( signatureDesc, GetPipelinePreset( PipelinePresets::MeshTranslucentTwoSided ) );
  }
}

RenderManager::~RenderManager()
{
  // We need to destruct all objects in the correct order!

  commandQueueManager->IdleGPU();
  stagingResources.clear();
  uploadBuffers.clear();
  randomTexture.reset();
  globalTextureFeedbackBuffer.reset();
  globalTextureFeedbackReadbackBuffer.reset();
  textureStreamer.reset();

  commandQueueManager.reset();

  for ( auto& preset : pipelinePresets )
    preset.reset();

  swapchain.reset();
  device.reset();
  adapter.reset();
  factory.reset();
  window.reset();
}

void RenderManager::RecreateWindowSizeDependantResources( CommandList& commandList )
{
}

int RenderManager::GetMSAASamples() const
{
  return msaaSamples;
}

int RenderManager::GetMSAAQuality() const
{
  return msaaQuality - 1;
}

int RenderManager::Get2DTexture( CommandQueueType commandQueueType, CommandList& commandList, const eastl::wstring& path, int* refTexutreId )
{
  textureStreamer->CacheTexture( commandQueueManager->GetQueue( commandQueueType ), commandList, path );
  return textureStreamer->GetTextureSlot( path, refTexutreId );
}

int RenderManager::Get2DTextureCount() const
{
  return textureStreamer->Get2DTextureCount();
}

TextureStreamer::MemoryStats RenderManager::GetMemoryStats() const
{
  return textureStreamer->GetMemoryStats();
}
