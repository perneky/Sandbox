#pragma once

#include "Types.h"
#include "Common/Finally.h"

struct FileLoaderFile;
struct CommandList;
struct CommandQueue;
struct Device;
struct ResourceDescriptor;

struct Resource
{
  virtual ~Resource() = default;

  virtual void                AttachResourceDescriptor( ResourceDescriptorType type, eastl::unique_ptr< ResourceDescriptor > descriptor ) = 0;
  virtual ResourceDescriptor* GetResourceDescriptor( ResourceDescriptorType type ) = 0;
  virtual void                RemoveResourceDescriptor( ResourceDescriptorType type ) = 0;
  virtual void                RemoveAllResourceDescriptors() = 0;

  virtual ResourceState GetCurrentResourceState() const = 0;

  virtual ResourceType GetResourceType() const = 0;
  virtual bool IsUploadResource() const = 0;

  virtual int GetBufferSize() const = 0;

  virtual int GetTextureWidth() const = 0;
  virtual int GetTextureHeight() const = 0;
  virtual int GetTextureDepthOrArraySize() const = 0;
  virtual int GetTextureMipLevels() const = 0;
  virtual PixelFormat GetTexturePixelFormat() const = 0;

  virtual uint64_t GetVirtualAllocationSize() const = 0;
  virtual uint64_t GetPhysicalAllocationSize() const = 0;

  virtual void* Map() = 0;
  virtual void  Unmap() = 0;

  virtual void UploadLoadedTiles( Device& device, CommandList& commandList ) = 0;

  virtual void EndFeedback( CommandQueue& commandQueue, Device& device, CommandList& commandList, uint64_t fence, uint64_t frameNo, int globalFeedback ) = 0;

  virtual FileLoaderFile* GetLoader() = 0;
};