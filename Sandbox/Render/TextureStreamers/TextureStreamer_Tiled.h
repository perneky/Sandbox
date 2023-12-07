#pragma once

#include "TextureStreamer.h"

struct Device;
struct Resource;
struct TileHeap;
struct FileLoader;

class TiledTextureStreamer : public TextureStreamer
{
public:
  TiledTextureStreamer( Device& device );
  ~TiledTextureStreamer();

  void CacheTexture( CommandQueue& commandQueue, CommandList& commandList, const eastl::wstring& path );

  int GetTextureSlot( const eastl::wstring& path, int* refTexutreId ) override;
  Resource* GetTexture( int index ) override;

  void UpdateBeforeFrame( Device& device, CommandList& commandList ) override;
  UpdateResult UpdateAfterFrame( Device& device, CommandQueue& commandQueue, CommandList& syncCommandList, uint64_t fence, uint32_t* globalFeedback ) override;

  int Get2DTextureCount() const override;

  MemoryStats GetMemoryStats() const override;

private:
  eastl::vector_map< eastl::wstring, int >       textureMap;
  eastl::vector< eastl::unique_ptr< Resource > > textures;
  eastl::vector< eastl::unique_ptr< TileHeap > > memoryHeaps;
  eastl::unique_ptr< FileLoader >                fileLoader;

  uint64_t frameNo = 0;
};