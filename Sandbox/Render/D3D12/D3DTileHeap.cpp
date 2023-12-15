#include "D3DTileHeap.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "D3DCommandList.h"
#include "D3DCommandQueue.h"

static constexpr uint64_t blockSize = 64 * 1024;

static int nextSlot = 0;

static int UI( int x, int y )
{
  return y * TileCount + x;
}
static eastl::pair< int, int > IU( int i )
{
  return { i % TileCount, i / TileCount };
}

D3DTileHeap::Texture::Texture( eastl::unique_ptr< Resource >&& resource, eastl::unique_ptr< MemoryHeap >&& heap )
  : resource( eastl::forward< eastl::unique_ptr< Resource > >( resource ) )
  , heap( eastl::forward< eastl::unique_ptr< MemoryHeap > >( heap ) )
  , freeTileCount( TileCount* TileCount )
{
  usage.resize( freeTileCount );
  ZeroMemory( usage.data(), usage.size() );
}

D3DTileHeap::D3DTileHeap( D3DDevice& device, PixelFormat pixelFormat, const wchar_t* debugName )
  : pixelFormat( pixelFormat )
{
  InitializeCriticalSectionAndSpinCount( &allocationLock, 4000 );
}

D3DTileHeap::Texture* D3DTileHeap::AllocateTexture( Device& device, CommandQueue& directQueue )
{
  int slot = nextSlot++;

  auto heap    = device.CreateMemoryHeap( TileCount * TileCount * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES, L"Tile heap memory" );
  auto texture = device.CreateReserved2DTexture( TileCount * CalcTileWidth( pixelFormat )
                                               , TileCount * CalcTileHeight( pixelFormat )
                                               , pixelFormat
                                               , Engine2DTileTexturesBaseSlot + slot
                                               , false
                                               , L"TileHeapTexture" );

  // Map the heap to all the tiles
  for ( int ty = 0; ty < TileCount; ++ty )
    for ( int tx = 0; tx < TileCount; ++tx )
      directQueue.UpdateTileMapping( *texture, tx, ty, 0, heap.get(), ty * TileCount + tx );

  auto texPtr = texture.get();
  auto iter = textures.emplace( texPtr, Texture( eastl::move( texture ), eastl::move( heap ) ) );

  return &iter.first->second;
}

D3DTileHeap::~D3DTileHeap()
{
  DeleteCriticalSection( &allocationLock );
}

void D3DTileHeap::prealloc( Device& device, CommandQueue& directQueue, int sizeMB )
{
  int64_t sizeByte = sizeMB * 1024 * 1024;

  while ( sizeByte > 0 )
  {
    auto texture = AllocateTexture( device, directQueue );
    sizeByte -= texture->resource->GetVirtualAllocationSize();
  }
}

TileHeap::Allocation D3DTileHeap::alloc( Device& device, CommandQueue& directQueue )
{
  EnterCriticalSection( &allocationLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &allocationLock ); } );

  Allocation allocation;
  allocation.tileHeap = this;

  // Find an empty slot
  for ( auto& texture : textures )
  {
    if ( texture.second.freeTileCount == 0 )
      continue;

    int cnt = 0;
    for ( auto& usage : texture.second.usage )
    {
      if ( usage == 0 )
      {
        usage = 1;
        --texture.second.freeTileCount;

        allocation.texture     = texture.second.resource.get();
        allocation.memoryHeap  = texture.second.heap.get();
        allocation.x           = IU( cnt ).first;
        allocation.y           = IU( cnt ).second;

        return allocation;
      }

      ++cnt;
    }

    assert( false );
  }

  auto newTexture = AllocateTexture( device, directQueue );
  assert( newTexture );

  newTexture->usage[ UI( 0, 0 ) ] = 1;
  newTexture->freeTileCount--;

  allocation.texture     = newTexture->resource.get();
  allocation.memoryHeap  = newTexture->heap.get();
  allocation.x           = 0;
  allocation.y           = 0;

  return allocation;
}

void D3DTileHeap::free( Allocation allocation )
{
  assert( allocation.texture );
  assert( allocation.tileHeap == this );

  EnterCriticalSection( &allocationLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &allocationLock ); } );

  auto iter = textures.find( allocation.texture );
  assert( iter != textures.end() );

  auto& usage = iter->second.usage[ UI( allocation.x, allocation.y ) ];
  assert( usage );
  usage = 0;
  ++iter->second.freeTileCount;
}
