#pragma once

#include "Types.h"

struct Device;
struct CommandQueue;

class CommandAllocatorPool;

class CommandQueueManager
{
  friend class CommandContext;

public:
  CommandQueueManager( Device& device );
  ~CommandQueueManager();

  CommandQueue&         GetQueue( CommandQueueType queueType );
  CommandAllocatorPool& GetAllocatorPool( CommandQueueType queueType );

  void IdleGPU();

private:
  eastl::unique_ptr< CommandQueue > graphicsQueue;
  eastl::unique_ptr< CommandQueue > computeQueue;
  eastl::unique_ptr< CommandQueue > copyQueue;

  eastl::unique_ptr< CommandAllocatorPool > graphicsCommandAllocatorPool;
  eastl::unique_ptr< CommandAllocatorPool > computeCommandAllocatorPool;
  eastl::unique_ptr< CommandAllocatorPool > copyCommandAllocatorPool;
};