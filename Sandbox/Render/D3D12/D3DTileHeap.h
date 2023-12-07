#pragma once

#include "../TileHeap.h"
#include "../ShaderValues.h"

enum class PixelFormat : uint32_t;

class D3DTileHeap : public TileHeap
{
  friend class D3DDevice;

public:
  ~D3DTileHeap();

  Allocation alloc( Device& device, CommandList& commandList ) override;
  void free( Allocation allocation ) override;

private:
  D3DTileHeap( D3DDevice& device, PixelFormat pixelFormat, const wchar_t* debugName );

  struct Texture
  {
    Texture() = default;
    Texture( eastl::unique_ptr< Resource >&& resource, int slot );

    eastl::unique_ptr< Resource > resource;
    int slot;
    eastl::array< uint8_t, TileCount * TileCount > usage;
    int freeTileCount;
  };

  Texture* AllocateTexture( Device& device, CommandList& commandList );

  eastl::vector_map< Resource*, Texture > textures;

  PixelFormat pixelFormat;

  CRITICAL_SECTION allocationLock;
};