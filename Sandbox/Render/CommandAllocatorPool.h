#pragma once

#include "Types.h"

struct Device;
struct CommandAllocator;

class CommandAllocatorPool
{
public:
  CommandAllocatorPool( CommandQueueType queueType );
  ~CommandAllocatorPool();

  CommandAllocator* RequestAllocator( Device& device, uint64_t completedFenceValue );
  void DiscardAllocator( uint64_t fenceValue, CommandAllocator* allocator );

  size_t Size() const;

private:
  CommandQueueType commandQueueType;

  eastl::vector< eastl::unique_ptr< CommandAllocator > >     allocatorPool;
  eastl::queue< eastl::pair< uint64_t, CommandAllocator* > > readyAllocators;
  eastl::mutex                                               allocatorMutex;
};