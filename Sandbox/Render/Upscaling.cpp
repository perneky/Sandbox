#include "Upscaling.h"
#include "ComputeShader.h"
#include "DLSSUpscaling.h"

eastl::unique_ptr< Upscaling > Upscaling::Instantiate()
{
  if ( DLSSUpscaling::IsAvailable() )
    return eastl::make_unique< DLSSUpscaling >();

  return nullptr;
}
