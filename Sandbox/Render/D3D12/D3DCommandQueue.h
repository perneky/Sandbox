#pragma once

#include "../CommandQueue.h"
#include "../Types.h"
#include "Common/AsyncJobThread.h"

struct Fence;

struct TileMappingJob
{
  Resource& resource;
  int tileX;
  int tileY;
  int mip;
  MemoryHeap* heap;
  int heapStartOffsetInTiles;
};

class D3DCommandQueue : public CommandQueue, public AsyncJobThread< TileMappingJob >
{
  friend class D3DDevice;

public:
  ~D3DCommandQueue();

  CommandQueueType GetCommandListType() const override;
  
  uint64_t IncrementFence() override;
  uint64_t GetNextFenceValue() override;
  uint64_t GetLastCompletedFenceValue() override;

  uint64_t Submit( eastl::vector< eastl::unique_ptr< CommandList > >& commandLists ) override;

  bool IsFenceComplete( uint64_t fenceValue ) override;
  
  void WaitForFence( uint64_t fenceValue ) override;
  void WaitForIdle() override;

  uint64_t GetFrequency() override;

  void UpdateTileMapping( Resource& resource, int tileX, int tileY, int mip, MemoryHeap* heap, int heapStartOffsetInTiles ) override;

  ID3D12CommandQueue* GetD3DCommandQueue();

private:
  D3DCommandQueue( D3DDevice& device, CommandQueueType type );

  void DoUpdateTileMapping( Resource& resource, int tileX, int tileY, int mip, MemoryHeap* heap, int heapStartOffsetInTiles );

  CommandQueueType              type;
  CComPtr< ID3D12CommandQueue > d3dCommandQueue;
  CComPtr< ID3D12Fence1 >       d3dFence;

  uint64_t nextFenceValue          = 1;
  uint64_t lastCompletedFenceValue = 0;

  eastl::mutex fenceMutex;
  eastl::mutex eventMutex;

  HANDLE fenceEventHandle;
};
