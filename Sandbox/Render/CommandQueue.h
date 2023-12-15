#pragma once

#include "Types.h"

struct CommandList;
struct Resource;
struct MemoryHeap;

struct CommandQueue
{
  virtual ~CommandQueue() = default;

  virtual CommandQueueType GetCommandListType() const = 0;

  virtual uint64_t IncrementFence() = 0;
  virtual uint64_t GetNextFenceValue() = 0;
  virtual uint64_t GetLastCompletedFenceValue() = 0;

  virtual uint64_t Submit( eastl::vector< eastl::unique_ptr< CommandList > >& commandLists ) = 0;
  
  virtual bool IsFenceComplete( uint64_t fenceValue ) = 0;
  
  virtual void WaitForFence( uint64_t fenceValue ) = 0;
  virtual void WaitForIdle() = 0;

  virtual uint64_t GetFrequency() = 0;

  virtual void UpdateTileMapping( Resource& resource, int tileX, int tileY, int mip, MemoryHeap* heap, int heapStartOffsetInTiles ) = 0;
};
