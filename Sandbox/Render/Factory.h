#pragma once

struct Adapter;
struct Device;
struct Swapchain;
struct CommandQueue;
struct Window;

struct Factory
{
  static eastl::unique_ptr< Factory > Create();

  virtual ~Factory() = default;

  virtual bool IsVRRSupported() const = 0;

  virtual eastl::unique_ptr< Adapter >   CreateDefaultAdapter() = 0;
  virtual eastl::unique_ptr< Swapchain > CreateSwapchain( Device& device, CommandQueue& directQueue, Window& window ) = 0;
};
