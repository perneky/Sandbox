#include "D3DCommandQueue.h"
#include "D3DCommandList.h"
#include "D3DDevice.h"
#include "Conversion.h"
#include "WinPixEventRuntime/pix3.h"

D3DCommandQueue::D3DCommandQueue( D3DDevice& device, CommandQueueType type )
  : type( type )
{
  InitializeCriticalSectionAndSpinCount( &tileMappingLock, 4000 );

  D3D12_COMMAND_QUEUE_DESC desc = {};
  desc.Type     = Convert( type );
  desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;

  device.GetD3DDevice()->CreateCommandQueue( &desc, IID_PPV_ARGS( &d3dCommandQueue ) );

  device.GetD3DDevice()->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &d3dFence ) );
  d3dFence->SetName( L"D3DCommandQueue::d3dFence" );
  d3dFence->Signal( 0 );

  fenceEventHandle = CreateEvent( nullptr, false, false, nullptr );
}

D3DCommandQueue::~D3DCommandQueue()
{
  CloseHandle( fenceEventHandle );

  DeleteCriticalSection( &tileMappingLock );
}

ID3D12CommandQueue* D3DCommandQueue::GetD3DCommandQueue()
{
  return d3dCommandQueue;
}

void D3DCommandQueue::UpdateTileMapping( ID3D12Resource* resource, const D3D12_TILED_RESOURCE_COORDINATE& resourceRegionStartCoordinate, ID3D12Heap* heap, UINT heapRangeStartOffset )
{
  auto key = TileMappingKey( resource, heap );

  EnterCriticalSection( &tileMappingLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &tileMappingLock ); } );

  auto& bucket = pendingTileMappings[ key ];

  #if 0
    for ( auto& bucket : pendingTileMappings )
      if ( bucket.first.first == resource )
        for ( auto& coord : bucket.second.resourceRegionStartCoordinates )
          assert( memcmp( &resourceRegionStartCoordinate, &coord, sizeof( CD3DX12_TILED_RESOURCE_COORDINATE ) ) != 0 );
  #endif

  bucket.resourceRegionStartCoordinates.emplace_back( resourceRegionStartCoordinate );
  bucket.tileRegionSizes.emplace_back( D3D12_TILE_REGION_SIZE { .NumTiles = 1, .UseBox = false } );
  bucket.rangeFlags.emplace_back( heap ? D3D12_TILE_RANGE_FLAG_NONE : D3D12_TILE_RANGE_FLAG_NULL );
  bucket.heapRangeStartOffsets.emplace_back( heapRangeStartOffset );
  bucket.rangeTileCounts.emplace_back( 1 );
}

CommandQueueType D3DCommandQueue::GetCommandListType() const
{
  return type;
}

void D3DCommandQueue::WaitForFence( uint64_t fenceValue )
{
  if ( IsFenceComplete( fenceValue ) )
    return;

  PIXBeginEvent( PIX_COLOR_DEFAULT, L"Wait for fence" );

  PIXBeginEvent( PIX_COLOR_DEFAULT, L"Lock" );
    eastl::lock_guard< eastl::mutex > lockGuard( eventMutex );
  PIXEndEvent();

  d3dFence->SetEventOnCompletion( fenceValue, fenceEventHandle );

  PIXBeginEvent( PIX_COLOR_DEFAULT, L"Wait" );
    WaitForSingleObject( fenceEventHandle, INFINITE );
  PIXEndEvent();

  lastCompletedFenceValue = fenceValue;

  PIXEndEvent();
}

void D3DCommandQueue::WaitForIdle()
{
  WaitForFence( IncrementFence() );
}

uint64_t D3DCommandQueue::GetFrequency()
{
  UINT64 frequency;
  if FAILED( d3dCommandQueue->GetTimestampFrequency( &frequency ) )
    frequency = 1;

  return frequency;
}

uint64_t D3DCommandQueue::IncrementFence()
{
  eastl::lock_guard< eastl::mutex > lockGuard( fenceMutex );
  d3dCommandQueue->Signal( d3dFence, nextFenceValue );
  return nextFenceValue++;
}

uint64_t D3DCommandQueue::GetNextFenceValue()
{
  return nextFenceValue;
}

uint64_t D3DCommandQueue::GetLastCompletedFenceValue()
{
  return lastCompletedFenceValue;
}

uint64_t D3DCommandQueue::Submit( eastl::vector< eastl::unique_ptr< CommandList > >& commandLists )
{
  eastl::lock_guard< eastl::mutex > lockGuard( fenceMutex );

  PIXBeginEvent( d3dCommandQueue.p, PIX_COLOR_DEFAULT, L"Batch update tile mappings" );

  for ( auto& bucket : pendingTileMappings )
  {
    d3dCommandQueue->UpdateTileMappings( bucket.first.first
                                       , UINT( bucket.second.resourceRegionStartCoordinates.size() )
                                       , bucket.second.resourceRegionStartCoordinates.data()
                                       , nullptr
                                       , bucket.first.second
                                       , UINT( bucket.second.resourceRegionStartCoordinates.size() )
                                       , bucket.second.rangeFlags.data()
                                       , bucket.second.heapRangeStartOffsets.data()
                                       , bucket.second.rangeTileCounts.data()
                                       , D3D12_TILE_MAPPING_FLAG_NONE );
  }

  PIXEndEvent( d3dCommandQueue.p );

  pendingTileMappings.clear();

  ID3D12CommandList* d3dCommandLists[ 8 ] = {};

  assert( _countof( d3dCommandLists ) >= commandLists.size() );

  int counter = 0;
  for ( auto& commandList : commandLists )
  {
    auto d3dGraphicsCommandList = static_cast< D3DCommandList& >( *commandList ).GetD3DGraphicsCommandList();
    if ( d3dGraphicsCommandList )
    {
      auto hr = d3dGraphicsCommandList->Close();
      assert( SUCCEEDED( hr ) );
    }

    d3dCommandLists[ counter++ ] = d3dGraphicsCommandList;
  }

  d3dCommandQueue->ExecuteCommandLists( counter, d3dCommandLists );

  d3dCommandQueue->Signal( d3dFence, nextFenceValue );

  return nextFenceValue++;
}

bool D3DCommandQueue::IsFenceComplete( uint64_t fenceValue )
{
  if ( fenceValue > lastCompletedFenceValue )
    lastCompletedFenceValue = eastl::max( lastCompletedFenceValue, d3dFence->GetCompletedValue() );

  return fenceValue <= lastCompletedFenceValue;
}
