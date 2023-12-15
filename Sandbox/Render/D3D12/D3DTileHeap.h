#pragma once

#include "../TileHeap.h"
#include "../ShaderValues.h"
#include "../MemoryHeap.h"

enum class PixelFormat : uint32_t;

class D3DTileHeap : public TileHeap
{
  friend class D3DDevice;

public:
  ~D3DTileHeap();

  void prealloc( Device& device, CommandQueue& directQueue, int sizeMB ) override;

  Allocation alloc( Device& device, CommandQueue& directQueue ) override;
  void free( Allocation allocation ) override;

private:
  D3DTileHeap( D3DDevice& device, PixelFormat pixelFormat, const wchar_t* debugName );

  struct Texture
  {
    Texture() = default;
    Texture( eastl::unique_ptr< Resource >&& resource, eastl::unique_ptr< MemoryHeap >&& heap );

    eastl::unique_ptr< Resource > resource;
    eastl::vector< uint8_t > usage;
    eastl::unique_ptr< MemoryHeap > heap;
    int freeTileCount;
  };

  Texture* AllocateTexture( Device& device, CommandQueue& directQueue );

  eastl::vector_map< Resource*, Texture > textures;
  
  PixelFormat pixelFormat;

  CRITICAL_SECTION allocationLock;
};