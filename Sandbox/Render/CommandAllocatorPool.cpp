#include "CommandAllocatorPool.h"
#include "CommandAllocator.h"
#include "Device.h"

CommandAllocatorPool::CommandAllocatorPool( CommandQueueType queueType )
  : commandQueueType( queueType )
{
}

CommandAllocatorPool::~CommandAllocatorPool()
{
}

CommandAllocator* CommandAllocatorPool::RequestAllocator( Device& device, uint64_t completedFenceValue )
{
  eastl::lock_guard< eastl::mutex > lockGuard( allocatorMutex );

  CommandAllocator* allocator = nullptr;

  if ( !readyAllocators.empty() )
  {
    eastl::pair< uint64_t, CommandAllocator* >& allocatorPair = readyAllocators.front();

    if ( allocatorPair.first <= completedFenceValue )
    {
      allocator = allocatorPair.second;
      allocator->Reset();
      readyAllocators.pop();
    }
  }

  if ( !allocator )
  {
    allocatorPool.emplace_back( device.CreateCommandAllocator( commandQueueType ) );
    allocator = allocatorPool.back().get();
  }

  return allocator;
}

void CommandAllocatorPool::DiscardAllocator( uint64_t fenceValue, CommandAllocator* allocator )
{
  eastl::lock_guard< eastl::mutex > lockGuard( allocatorMutex );

  readyAllocators.push( eastl::make_pair( fenceValue, allocator ) );
}

size_t CommandAllocatorPool::Size() const
{
  return size_t();
}
