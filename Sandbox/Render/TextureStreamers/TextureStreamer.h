#pragma once

struct CommandAllocator;
struct CommandList;
struct CommandQueue;
struct Device;
struct Resource;

struct TextureStreamer
{
  virtual ~TextureStreamer() = default;

  virtual void CacheTexture( CommandQueue& commandQueue, CommandList& commandList, const eastl::wstring& path ) = 0;

  virtual int GetTextureSlot( const eastl::wstring& path, int* refTexutreId = nullptr ) = 0;
  virtual Resource* GetTexture( int index ) = 0;

  using UpdateResult = eastl::vector< eastl::pair< eastl::unique_ptr< CommandList >, CommandAllocator* > >;

  virtual void UpdateBeforeFrame( Device& device, CommandList& commandList ) = 0;
  virtual UpdateResult UpdateAfterFrame( Device& device, CommandQueue& commandQueue, CommandList& syncCommandList, uint64_t fence, uint32_t* globalFeedback ) = 0;

  virtual int Get2DTextureCount() const = 0;

  struct MemoryStats
  {
    int      textureCount;
    uint64_t virtualAllocationSize;
    uint64_t physicalAllocationSize;
  };
  virtual MemoryStats GetMemoryStats() const = 0;
};