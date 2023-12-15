#pragma once

#ifndef TEXTURE_FEEDBACK_HLSLI
#define TEXTURE_FEEDBACK_HLSLI

#include "../../ShaderValues.h"

half4 SampleTexture( uint index, float2 tc )
{
  return scene2DTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, 4 );
}

half4 SampleTexture( uint index, float2 tc, half mipLevel )
{
  #if TEXTURE_STREAMING_MODE == TEXTURE_STREAMING_OFF
    return scene2DTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, mipLevel );
  #elif TEXTURE_STREAMING_MODE == TEXTURE_STREAMING_RESERVED
    half4 texel;
    uint status;
    bool fullyMapped = false;
    while ( !fullyMapped )
    {
      texel = scene2DTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, mipLevel, 0, status );
      fullyMapped = CheckAccessFullyMapped( status );
      ++mipLevel;
    }
    return texel;
  #endif
}

half4 SampleTexture( uint index, float2 tc, half mipLevel, uint2 screenPosition, uint feedbackPhase )
{
  #if TEXTURE_STREAMING_MODE != TEXTURE_STREAMING_OFF && ENABLE_TEXTURE_FEEDBACK
    if ( feedbackPhase && ( screenPosition.x % 10 ) == feedbackPhase - 1 )
    {
      float2 dim;
      scene2DTexturesFeedback[ index ].GetDimensions( dim.x, dim.y );
      InterlockedMin( scene2DTexturesFeedback[ index ][ uint2( frac( tc ) * dim ) ], uint( mipLevel ) );

      globalTextureFeedback.InterlockedMin( index * 4, uint( mipLevel ) );
    }
  #endif
  
  return SampleTexture( index, tc, mipLevel );
}

#endif