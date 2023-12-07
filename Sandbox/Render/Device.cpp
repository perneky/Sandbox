#include "Device.h"
#include "Resource.h"

eastl::unique_ptr< Resource > Device::Create2DTexture( CommandList& commandList, int width, int height, const void* data, int dataSize, PixelFormat format, bool renderable, int slot, eastl::optional< int > uavSlot, bool mipLevels, const wchar_t* debugName )
{
  return Create2DTexture( commandList, width, height, data, dataSize, format, 1, 0, renderable, slot, uavSlot, mipLevels, debugName );
}
