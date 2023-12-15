#pragma once

struct CommandList;
struct Device;
struct Resource;
struct MemoryHeap;
struct CommandQueue;

struct TileHeap
{
  static constexpr uint64_t InvalidAllocation = 0xFFFFFFFFFFFFFFFFLLU;

  virtual ~TileHeap() = default;

  struct Allocation
  {
    TileHeap*   tileHeap    = nullptr;
    Resource*   texture     = nullptr;
    MemoryHeap* memoryHeap  = nullptr;
    int16_t     x           = -1;
    int16_t     y           = -1;
  };

  virtual void prealloc( Device& device, CommandQueue& directQueue, int sizeMB ) = 0;

  virtual Allocation alloc( Device& device, CommandQueue& directQueue ) = 0;
  virtual void free( Allocation allocation ) = 0;
};