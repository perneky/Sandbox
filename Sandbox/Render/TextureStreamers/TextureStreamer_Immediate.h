#pragma once

#include "TextureStreamer.h"

struct Resource;

class ImmediateTextureStreamer : public TextureStreamer
{
public:
  ImmediateTextureStreamer() = default;
  ~ImmediateTextureStreamer() = default;

  void CacheTexture( CommandQueue& directQueue, CommandList& commandList, const eastl::wstring& path ) override;

  int GetTextureSlot( const eastl::wstring& path, int* refTexutreId ) override;
  Resource* GetTexture( int index ) override;

  void UpdateBeforeFrame( Device& device, CommandQueue& copyQueue, CommandList& commandList ) override;
  UpdateResult UpdateAfterFrame( Device& device, CommandQueue& graphicsQueue, CommandQueue& copyQueue, CommandList& syncCommandList, uint64_t fence, uint32_t* globalFeedback ) override;

  int Get2DTextureCount() const override;

  MemoryStats GetMemoryStats() const override;

private:
  eastl::vector_map< eastl::wstring, int >       textureMap;
  eastl::vector< eastl::unique_ptr< Resource > > textures;
};