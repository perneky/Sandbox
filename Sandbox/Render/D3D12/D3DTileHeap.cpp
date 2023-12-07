#include "D3DTileHeap.h"
#include "D3DDevice.h"
#include "D3DResource.h"

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

D3DTileHeap::Texture::Texture( eastl::unique_ptr< Resource >&& resource, int slot )
  : resource( eastl::forward< eastl::unique_ptr< Resource > >( resource ) )
  , slot( slot )
  , freeTileCount( TileCount * TileCount )
{
  ZeroMemory( usage.data(), usage.size() );
}

D3DTileHeap::D3DTileHeap( D3DDevice& device, PixelFormat pixelFormat, const wchar_t* debugName )
  : pixelFormat( pixelFormat )
{
  InitializeCriticalSectionAndSpinCount( &allocationLock, 4000 );
}

D3DTileHeap::Texture* D3DTileHeap::AllocateTexture( Device& device, CommandList& commandList )
{
  int slot = nextSlot++;

  auto texture = device.Create2DTexture( commandList
                                       , TileTextureSize
                                       , TileTextureSize
                                       , nullptr
                                       , 0
                                       , pixelFormat
                                       , false
                                       , Engine2DTileTexturesBaseSlot + slot
                                       , eastl::nullopt
                                       , false
                                       , L"TileHeapTexture" );

  auto texPtr = texture.get();
  auto iter   = textures.emplace( texPtr, Texture( eastl::move( texture ), slot ) );

  return &iter.first->second;
}

D3DTileHeap::~D3DTileHeap()
{
  DeleteCriticalSection( &allocationLock );
}

TileHeap::Allocation D3DTileHeap::alloc( Device& device, CommandList& commandList )
{
  EnterCriticalSection( &allocationLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &allocationLock ); } );

  Allocation allocation;
  allocation.heap = this;

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
        allocation.textureSlot = texture.second.slot;
        allocation.x           = IU( cnt ).first;
        allocation.y           = IU( cnt ).second;

        return allocation;
      }

      ++cnt;
    }

    assert( false );
  }

  auto newTexture = AllocateTexture( device, commandList );
  assert( newTexture );

  newTexture->usage[ UI( 0, 0 ) ] = 1;
  newTexture->freeTileCount--;

  allocation.texture     = newTexture->resource.get();
  allocation.textureSlot = newTexture->slot;
  allocation.x           = 0;
  allocation.y           = 0;

  return allocation;
}

void D3DTileHeap::free( Allocation allocation )
{
  assert( allocation.texture );
  assert( allocation.heap == this );

  EnterCriticalSection( &allocationLock );
  auto unlock = eastl::make_finally( [this]() { LeaveCriticalSection( &allocationLock ); } );

  auto iter = textures.find( allocation.texture );
  assert( iter != textures.end() );

  auto& usage = iter->second.usage[ UI( allocation.x, allocation.y ) ];
  assert( usage );
  usage = 0;
  ++iter->second.freeTileCount;
}
