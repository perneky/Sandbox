#pragma once

struct CommandList;
struct Device;
struct Resource;

struct TileHeap
{
  static constexpr uint64_t InvalidAllocation = 0xFFFFFFFFFFFFFFFFLLU;

  virtual ~TileHeap() = default;

  struct Allocation
  {
    TileHeap* heap        = nullptr;
    Resource* texture     = nullptr;
    int       textureSlot = -1;
    int       x           = -1;
    int       y           = -1;
  };

  virtual Allocation alloc( Device& device, CommandList& commandList ) = 0;
  virtual void free( Allocation allocation ) = 0;
};