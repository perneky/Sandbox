#pragma once

struct Device;

struct Adapter
{
  virtual ~Adapter() = default;

  virtual eastl::unique_ptr< Device > CreateDevice() = 0;
};
