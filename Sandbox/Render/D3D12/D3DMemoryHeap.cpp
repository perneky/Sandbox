#include "D3DMemoryHeap.h"
#include "D3DDevice.h"

static constexpr uint64_t blockSize = 64 * 1024;

D3DMemoryHeap::D3DMemoryHeap( D3DDevice& device, uint64_t size, const wchar_t* debugName )
  : size( size )
{
  InitializeCriticalSectionAndSpinCount( &allocationLock, 4000 );

  assert( size % blockSize == 0 );

  D3D12_HEAP_DESC heapDesc = {};
  heapDesc.SizeInBytes                     = size;
  heapDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
  heapDesc.Flags                           = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
  heapDesc.Properties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

  device.GetD3DDevice()->CreateHeap( &heapDesc, IID_PPV_ARGS( &d3dHeap ) );
  assert( d3dHeap );

  if ( debugName )
    d3dHeap->SetName( debugName );

  auto blockCount = size / blockSize;
  allocations.resize( blockCount / 64 );
  allocationLinks.resize( blockCount / 64 );

  ZeroMemory( allocations.data(), allocations.size() * 8 );
  ZeroMemory( allocationLinks.data(), allocationLinks.size() * 8 );
}

D3DMemoryHeap::~D3DMemoryHeap()
{
  DeleteCriticalSection( &allocationLock );
}

ID3D12Heap* D3DMemoryHeap::GetD3DHeap()
{
  return d3dHeap;
}

uint64_t D3DMemoryHeap::alloc( uint64_t size )
{
  assert( size % blockSize == 0 );
  assert( size == blockSize ); // Only ready for this for now

  uint64_t blockCount = size / blockSize;

  uint64_t startAllocIndex = 0;
  uint64_t startAllocBit   = 0;

  EnterCriticalSection( &allocationLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &allocationLock ); } );

  int allocIndex = 0;
  for ( auto& allocs : allocations )
  {
    if ( allocs != 0xFFFFFFFFFFFFFFFFLLU )
    {
      for ( int bitIndex = 0; bitIndex < 64; ++bitIndex )
      {
        uint64_t mask = 1LLU << ( 64 - bitIndex - 1 );

        if ( ( allocs & mask ) == 0 )
        {
          allocs |= mask;
          // Always a single block for now

          return ( allocIndex * 64 + bitIndex ) * blockSize;
        }
      }
    }

    ++allocIndex;
  }

  return InvalidAllocation;
}

void D3DMemoryHeap::free( uint64_t address )
{
  assert( address % blockSize == 0 );

  uint64_t blockIndex = address / blockSize;
  uint64_t allocIndex = blockIndex / 64;
  uint64_t bitIndex   = blockIndex % 64;

  EnterCriticalSection( &allocationLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &allocationLock ); } );

  while ( true )
  {
    uint64_t mask  = 1LLU << ( 64 - bitIndex - 1 );

    bool taken  = allocations[ allocIndex ] & mask;
    bool linked = allocationLinks[ allocIndex ] & mask;

    assert( taken );

    allocations[ allocIndex ] &= ~mask;
    allocationLinks[ allocIndex ] &= ~mask;

    if ( !linked )
      break;

    ++bitIndex;
    if ( bitIndex == 64 )
    {
      bitIndex = 0;
      allocIndex++;
    }
  }
}

