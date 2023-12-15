#include "D3DCommandQueue.h"
#include "D3DCommandList.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "D3DMemoryHeap.h"
#include "Conversion.h"
#include "WinPixEventRuntime/pix3.h"

D3DCommandQueue::D3DCommandQueue( D3DDevice& device, CommandQueueType type )
  : type( type )
{
  D3D12_COMMAND_QUEUE_DESC desc = {};
  desc.Type     = Convert( type );
  desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  desc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;

  device.GetD3DDevice()->CreateCommandQueue( &desc, IID_PPV_ARGS( &d3dCommandQueue ) );

  device.GetD3DDevice()->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &d3dFence ) );
  d3dFence->SetName( L"D3DCommandQueue::d3dFence" );
  d3dFence->Signal( 0 );

  fenceEventHandle = CreateEvent( nullptr, false, false, nullptr );

  if ( type == CommandQueueType::Copy )
    Start( [this]( TileMappingJob& job )
    {
      DoUpdateTileMapping( job.resource, job.tileX, job.tileY, job.mip, job.heap, job.heapStartOffsetInTiles );
    }, "D3DCommandQueueTileMapping");
}

void D3DCommandQueue::DoUpdateTileMapping( Resource& resource, int tileX, int tileY, int mip, MemoryHeap* heap, int heapStartOffsetInTiles )
{
  auto& d3dResource   = static_cast< D3DResource&   >( resource );
  auto  d3dMemoryHeap = static_cast< D3DMemoryHeap* >( heap );

  PIXBeginEvent( d3dCommandQueue.p, PIX_COLOR_DEFAULT, L"Update tile mapping" );

  auto coord = D3D12_TILED_RESOURCE_COORDINATE { .X = UINT( tileX ), .Y = UINT( tileY ), .Z = 0, .Subresource = UINT( mip ) };
  auto flags = d3dMemoryHeap ? D3D12_TILE_RANGE_FLAG_NONE : D3D12_TILE_RANGE_FLAG_NULL;
  auto count = 1U;

  d3dCommandQueue->UpdateTileMappings( d3dResource.GetD3DResource()
                                     , 1
                                     , &coord
                                     , nullptr
                                     , d3dMemoryHeap ? d3dMemoryHeap->GetD3DHeap() : nullptr
                                     , 1
                                     , &flags
                                     , (UINT*)&heapStartOffsetInTiles
                                     , &count
                                     , D3D12_TILE_MAPPING_FLAG_NONE );

  PIXEndEvent( d3dCommandQueue.p );
}

D3DCommandQueue::~D3DCommandQueue()
{
  CloseHandle( fenceEventHandle );
}

ID3D12CommandQueue* D3DCommandQueue::GetD3DCommandQueue()
{
  return d3dCommandQueue;
}

void D3DCommandQueue::UpdateTileMapping( Resource& resource, int tileX, int tileY, int mip, MemoryHeap* heap, int heapStartOffsetInTiles )
{
  if ( type == CommandQueueType::Copy )
    Enqueue( TileMappingJob { .resource = resource, .tileX = tileX, .tileY = tileY, .mip = mip, .heap = heap, .heapStartOffsetInTiles = heapStartOffsetInTiles } );
  else
    DoUpdateTileMapping( resource, tileX, tileY, mip, heap, heapStartOffsetInTiles );
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
