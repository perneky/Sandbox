#pragma once

#include "../Swapchain.h"

class WinAPIWindow;
class D3DCommandQueue;
class D3DDevice;
class D3DResource;

class D3DSwapchain : public Swapchain
{
  friend class D3DFactory;

public:
  ~D3DSwapchain();

  void BuildBackBufferTextures( Device& device ) override;

  Resource& GetCurrentBackBufferTexture() override;
  int GetCurrentBackBufferTextureIndex() override;

  uint64_t Present( uint64_t fenceValue, bool useVSync ) override;

  void ToggleFullscreen( CommandList& commandList, Device& device, Window& window ) override;
  void Resize( CommandList& commandList, Device& device, Window& window ) override;

  IDXGISwapChain4* GetDXGISwapchain();

private:
  D3DSwapchain( D3DFactory& factory, D3DCommandQueue& directQueue, D3DDevice& device, WinAPIWindow& window );

  CComPtr< IDXGISwapChain4 > dxgiSwapchain;

  CComPtr< ID3D12DescriptorHeap > d3dBackBufferDescriptors;

  struct FrameData
  {
    eastl::unique_ptr< D3DResource > texture;
    uint64_t                       fenceValue = 0;
  };

  bool isTearingSupported = false;
  bool isTearingEnabled   = false;
  bool isFullscreen       = false;

  eastl::vector< FrameData > frameData;
  size_t                     currentFrameIndex = 0;

  eastl::unique_ptr< D3DResource > depthTexture;
};
