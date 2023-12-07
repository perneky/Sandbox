#pragma once

#include "../CommandQueue.h"
#include "../Types.h"

struct Fence;

class D3DCommandQueue : public CommandQueue
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

  ID3D12CommandQueue* GetD3DCommandQueue();

  void UpdateTileMapping( ID3D12Resource* resource, const D3D12_TILED_RESOURCE_COORDINATE& resourceRegionStartCoordinate, ID3D12Heap* heap, UINT heapRangeStartOffset );

private:
  D3DCommandQueue( D3DDevice& device, CommandQueueType type );

  CommandQueueType              type;
  CComPtr< ID3D12CommandQueue > d3dCommandQueue;
  CComPtr< ID3D12Fence1 >       d3dFence;

  uint64_t nextFenceValue          = 1;
  uint64_t lastCompletedFenceValue = 0;

  eastl::mutex fenceMutex;
  eastl::mutex eventMutex;

  HANDLE fenceEventHandle;

  using TileMappingKey = eastl::pair< CComPtr< ID3D12Resource >, CComPtr< ID3D12Heap > >;
  struct TileMapping
  {
    eastl::vector < D3D12_TILED_RESOURCE_COORDINATE > resourceRegionStartCoordinates;
    eastl::vector < D3D12_TILE_REGION_SIZE > tileRegionSizes;
    eastl::vector < D3D12_TILE_RANGE_FLAGS > rangeFlags;
    eastl::vector < UINT > heapRangeStartOffsets;
    eastl::vector < UINT > rangeTileCounts;
  };

  eastl::map< TileMappingKey, TileMapping > pendingTileMappings;

  CRITICAL_SECTION tileMappingLock;
};
