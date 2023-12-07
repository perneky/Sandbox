#include "Platform/Window.h"
#include "Render/RenderManager.h"
#include "Render/CommandList.h"
#include "Render/Swapchain.h"
#include "Render/Mesh.h"
#include "Render/Utils.h"
#include "Render/ShaderStructures.h"
#include "Common/Color.h"
#include "Common/Finally.h"
#include "Common/Files.h"
#include "Scene/Scene.h"
#include "Sandbox.h"
#include "UI/Debug/DebugWindow.h"
#include "../DearImGui/imgui.h"
#include "../External/tinyxml2/tinyxml2.h"

/*
TODO:
o Improve depth pass performance
  o Sort models by depth after culling to improve depth prepass perf?
  o Do AABB occlusion culling?
  o Use mesh shaders and occlusion culling?
o Fix the map
  o Wrong facing of triangles
  + House parts rotated?
o Raycast camera
  o From camera pos, cast ray down on GPU, align camera on that
  o Also cast ray around to get stuck in walls
 */

#if 0
  static const wchar_t* scenePath = L"Content/UrbanScene";
#else
  static const wchar_t* scenePath = L"Content/FBX";
#endif

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  if ( enableImGui )
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();
  }

  eastl::shared_ptr< Window > window = Window::Create( 1920, 1080 );
  if ( RenderManager::CreateInstance( window ) )
  {
    {
      auto&    renderManager       = RenderManager::GetInstance();
      uint64_t nextFrameFenceValue = 0;

      auto commandAllocator = renderManager.RequestCommandAllocator( CommandQueueType::Direct );
      auto commandList      = renderManager.CreateCommandList( *commandAllocator, CommandQueueType::Direct );

      renderManager.SetUp( *commandList, scenePath );
      renderManager.RecreateWindowSizeDependantResources( *commandList );

      eastl::unique_ptr< Scene > scene = eastl::make_unique< Scene >( *commandList, scenePath, window->GetClientWidth(), window->GetClientHeight() );
      if ( !scene->GetError().empty() )
      {
        OutputDebugStringW( scene->GetError().data() );
        return -1;
      }

      bool windowResized = false;
      window->SigSizeChanged.Connect( [&]( int, int )
      {
        windowResized = true;
      } );

      window->SigKeyDown.Connect( [ & ]( int key )
      {
      } );

      window->SigKeyUp.Connect( [ & ]( int key )
      {
        } );

      Sandbox::SetupCameraControl( *window );

      auto     lastFrameTime = GetCPUTime();
      uint64_t frameCounter  = 0;

      DebugWindow debugWindow;

      renderManager.Submit( eastl::move( commandList ), CommandQueueType::Direct, true );

      auto shoudQuit = false;
      while ( !shoudQuit )
      {
        shoudQuit = !window->ProcessMessages();

        auto commandAllocator = renderManager.RequestCommandAllocator( CommandQueueType::Direct );
        auto commandList      = renderManager.CreateCommandList( *commandAllocator, CommandQueueType::Direct );
        GPUSection gpuCommandListSection( *commandList, L"Graphics command list" );

        if ( scene->GetUpscalingQuality() != debugWindow.GetUpscalingQuality() )
        {
          gpuCommandListSection.Close();

          renderManager.IdleGPU();

          scene->TearDownScreenSizeDependantTextures( *commandList );

          auto fenceValue = renderManager.Submit( eastl::move( commandList ), CommandQueueType::Direct, true );
          renderManager.DiscardCommandAllocator( CommandQueueType::Direct, commandAllocator, fenceValue );
          renderManager.TidyUp();
          renderManager.GetDevice().StartNewFrame();

          commandAllocator = renderManager.RequestCommandAllocator( CommandQueueType::Direct );
          commandList = renderManager.CreateCommandList( *commandAllocator, CommandQueueType::Direct );

          scene->SetUpscalingQuality( *commandList, window->GetClientWidth(), window->GetClientHeight(), debugWindow.GetUpscalingQuality() );

          renderManager.IdleGPU();
        }

        scene->SetManualExposure( debugWindow.GetManualExposure() );

        eastl::wstring frameName;
        GPUSection gpuFrameSection( *commandList, frameName.sprintf( L"Render frame %llu", frameCounter++ ).data() );

        if ( windowResized || Sandbox::toggleFullscreen )
        {
          gpuFrameSection.Close();
          gpuCommandListSection.Close();

          renderManager.IdleGPU();

          if ( Sandbox::toggleFullscreen )
            renderManager.GetSwapchain().ToggleFullscreen( *commandList, renderManager.GetDevice(), *window );
          else
            renderManager.GetSwapchain().Resize( *commandList, renderManager.GetDevice(), *window );

          // This is really not needed. Scene buffers only rebuilt to update camera projection.
          scene->TearDownSceneBuffers( *commandList );
          scene->TearDownScreenSizeDependantTextures( *commandList );

          auto fenceValue = renderManager.Submit( eastl::move( commandList ), CommandQueueType::Direct, true );
          renderManager.DiscardCommandAllocator( CommandQueueType::Direct, commandAllocator, fenceValue );
          renderManager.TidyUp();
          renderManager.GetDevice().StartNewFrame();

          commandAllocator = renderManager.RequestCommandAllocator( CommandQueueType::Direct );
          commandList = renderManager.CreateCommandList( *commandAllocator, CommandQueueType::Direct );

          scene->OnScreenResize( *commandList, window->GetClientWidth(), window->GetClientHeight() );

          windowResized = false;
          Sandbox::toggleFullscreen = false;

          renderManager.IdleGPU();
        }

        auto& backBuffer = renderManager.GetSwapchain().GetCurrentBackBufferTexture();

        auto thisFrameTime = GetCPUTime();
        auto timeElapsed = thisFrameTime - lastFrameTime;
        lastFrameTime = thisFrameTime;

        if ( enableImGui )
        {
          renderManager.GetDevice().DearImGuiNewFrame();
          window->DearImGuiNewFrame();
          ImGui::NewFrame();
        }

        {
          GPUSection gpuSection( *commandList, L"Prepare next swapchain buffer" );
          renderManager.PrepareNextSwapchainBuffer( nextFrameFenceValue );
        }

        if ( debugWindow.GetUpdateTextureStreaming() )
        {
          GPUSection gpuSection( *commandList, L"Update before frame" );
          renderManager.UpdateBeforeFrame( *commandList );
        }

        Sandbox::TickCamera( *commandList, *scene, timeElapsed );

        scene->Render( *commandAllocator
                     , *commandList
                     , backBuffer
                     , debugWindow.GetUpdateTextureStreaming()
                     , debugWindow.GetShowDenoiserDebugLayer()
                     , debugWindow.GetFreezeCulling() );

        TextureStreamer::UpdateResult streamingCommandLists;
        if ( debugWindow.GetUpdateTextureStreaming() )
          streamingCommandLists = renderManager.UpdateAfterFrame( *commandList, CommandQueueType::Direct, nextFrameFenceValue );

        if ( int texIndex = debugWindow.GetDebugTextureIndex(); texIndex >= 0 )
          renderManager.RenderDebugTexture( *commandList, texIndex, backBuffer.GetTextureWidth(), backBuffer.GetTextureHeight() );

        if ( enableImGui )
        {
          GPUSection gpuSection( *commandList, L"DearImGui" );

          commandList->ChangeResourceState( backBuffer, ResourceStateBits::RenderTarget );
          commandList->SetRenderTarget( backBuffer, nullptr );

          debugWindow.Tick( *commandList, timeElapsed );

          ImGui::Render();

          commandList->DearImGuiRender();
        }

        commandList->ChangeResourceState( backBuffer, ResourceStateBits::Present );

        gpuFrameSection.Close();
        gpuCommandListSection.Close();

        eastl::vector< eastl::unique_ptr< CommandList > > submitList;
        for ( auto& scl : streamingCommandLists )
          submitList.emplace_back( eastl::move( scl.first ) );
        submitList.emplace_back( eastl::move( commandList ) );
        auto fenceValue = renderManager.Submit( eastl::move( submitList ), CommandQueueType::Direct, false );
        for ( auto& scl : streamingCommandLists )
          renderManager.DiscardCommandAllocator( CommandQueueType::Direct, scl.second, fenceValue );
        renderManager.DiscardCommandAllocator( CommandQueueType::Direct, commandAllocator, fenceValue );
        nextFrameFenceValue = renderManager.Present( fenceValue, debugWindow.GetUseVSync() );

        renderManager.TidyUp();
        renderManager.GetDevice().StartNewFrame();
      }

      renderManager.IdleGPU();

      scene->TearDown( nullptr );
    }

    RenderManager::DeleteInstance();
  }

  if ( enableImGui )
    ImGui::DestroyContext();

  return 0;
}
