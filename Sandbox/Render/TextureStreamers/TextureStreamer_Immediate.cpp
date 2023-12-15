#include "TextureStreamer_Immediate.h"
#include "Common/Files.h"
#include "Render/Device.h"
#include "Render/RenderManager.h"
#include "Render/Resource.h"
#include "Render/CommandList.h"
#include "Render/ShaderValues.h"

void ImmediateTextureStreamer::CacheTexture( CommandQueue& directQueue, CommandList& commandList, const eastl::wstring& path )
{
  auto& device = RenderManager::GetInstance().GetDevice();

  int slot = int( textures.size() );
  assert( slot < Scene2DResourceCount );

  auto fileData = ReadFileToMemory( path.data() );
  if ( fileData.empty() )
    return;

  auto texture = device.Load2DTexture( commandList, eastl::move( fileData ), Scene2DResourceBaseSlot + slot, path.data() );
  if ( !texture )
    return;

  textures.emplace_back( eastl::move( texture ) );
  textureMap[ path ] = slot;
}

int ImmediateTextureStreamer::GetTextureSlot( const eastl::wstring& path, int* refTexutreId )
{
  if ( refTexutreId )
    *refTexutreId = -1;
  auto iter = textureMap.find( path );
  return iter == textureMap.end() ? -1 : iter->second;
}

Resource* ImmediateTextureStreamer::GetTexture( int index )
{
  return textures[ index ].get();
}

void ImmediateTextureStreamer::UpdateBeforeFrame( Device& device, CommandQueue& copyQueue, CommandList& commandList )
{
}

TextureStreamer::UpdateResult ImmediateTextureStreamer::UpdateAfterFrame( Device& device, CommandQueue& graphicsQueue, CommandQueue& copyQueue, CommandList& syncCommandList, uint64_t fence, uint32_t* globalFeedback )
{
  return {};
}

int ImmediateTextureStreamer::Get2DTextureCount() const
{
  return int( textures.size() );
}

ImmediateTextureStreamer::MemoryStats ImmediateTextureStreamer::GetMemoryStats() const
{
  MemoryStats stats = {};

  stats.textureCount = int( textures.size() );

  for ( auto& texture : textures )
  {
    stats.virtualAllocationSize  += texture->GetVirtualAllocationSize ();
    stats.physicalAllocationSize += texture->GetPhysicalAllocationSize();
  }

  return stats;
}
