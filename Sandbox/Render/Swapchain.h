#pragma once

struct Device;
struct Resource;
struct CommandList;
struct Window;

struct Swapchain
{
  virtual ~Swapchain() = default;

  virtual void BuildBackBufferTextures( Device& device ) = 0;

  virtual Resource& GetCurrentBackBufferTexture() = 0;
  virtual int GetCurrentBackBufferTextureIndex() = 0;

  virtual uint64_t Present( uint64_t fenceValue, bool useVSync ) = 0;

  virtual void ToggleFullscreen( CommandList& commandList, Device& device, Window& window ) = 0;
  virtual void Resize( CommandList& commandList, Device& device, Window& window ) = 0;
};
