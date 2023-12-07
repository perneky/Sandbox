#pragma once

struct RTInstance;
struct CommandList;
struct Device;
struct ResourceDescriptor;

struct RTTopLevelAccelerator
{
  virtual ~RTTopLevelAccelerator() = default;

  virtual void Update( Device& device, CommandList& commandList, eastl::vector< RTInstance > instances ) = 0;

  virtual ResourceDescriptor& GetResourceDescriptor() = 0;
};