#pragma once

#include "Types.h"

struct Device;
struct Resource;
struct ResourceDescriptor;

struct DescriptorHeap
{
  virtual ~DescriptorHeap() = default;

  virtual eastl::unique_ptr< ResourceDescriptor > RequestDescriptorFromSlot( Device& device, ResourceDescriptorType type, int slot, Resource& resource, int bufferElementSize, int mipLevel = 0 ) = 0;
  virtual eastl::unique_ptr< ResourceDescriptor > RequestDescriptorAuto( Device& device, ResourceDescriptorType type, int base, Resource& resource, int bufferElementSize, int mipLevel = 0 ) = 0;

  virtual int GetDescriptorSize() const = 0;
};