#pragma once

#ifndef TEXTURE_FEEDBACK_HLSLI
#define TEXTURE_FEEDBACK_HLSLI

#include "../../ShaderValues.h"

#if ENABLE_TEXTURE_STREAMING
bool TrySampleTextureLevel( uint index, uint mip, float2 tc, uint2 texSize, float noLevels, out half4 sample )
{
  if ( mip >= noLevels - 1 )
  {
    // If we are to sample beyond the index's last mip, or the last mip, then it is the mip tail
    uint mipOffset = scene2DTextures[ index ].Load( uint3( 0, 0, noLevels - 1 ) ).x;
    sample = scene2DMipTailTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, half( mip - mipOffset ) );
    return true;
  }
 
  // Check if the index texture has this
  int2 tci = tc * texSize;
  uint2 tileInfo = scene2DTextures[ index ].Load( uint3( tci, mip ) );
  if ( tileInfo.y == 0xFFFE )
  {
    sample = 0;
    return false;
  }
  
  uint  tileTextureIndex = tileInfo.y;
  uint2 tileCoord        = uint2( tileInfo.x & 255, tileInfo.x >> 8 );
  
  float2 pc = fmod( tc * texSize * TileSizeF, TileSizeF );
  pc /= TileTextureSizeF;
  pc += tileCoord * ( TileSizeWithBorderF / TileTextureSizeF );
  pc += 4 / TileTextureSizeF;
  
  sample = engine2DTileTextures[ tileTextureIndex ].SampleLevel( trilinearWrapSampler, pc, 0 );

  return true;
}
#endif

half4 SampleTexture( uint index, float2 tc )
{
  #if ENABLE_TEXTURE_STREAMING
    return scene2DMipTailTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, 0 );
  #else
    return scene2DTextures[ index ].SampleLevel( scene2DSampler, tc, 4 );
  #endif
}

half4 SampleTexture( uint index, float2 tc, half mipLevel )
{
  #if ENABLE_TEXTURE_STREAMING
    tc = frac( tc ); // Implement wrapping
    uint mipHi = floor( mipLevel );
    uint mipLo = ceil( mipLevel );
  
    uint2 texSizeHi;
    float noLevels;
    scene2DTextures[ index ].GetDimensions( mipHi, texSizeHi.x, texSizeHi.y, noLevels );

    if ( mipLevel >= noLevels - 1 )
    {
      // If it is immediately in the mip tail, then just sample it as it is
      // Similar to TrySampleTextureLevel, but the mip here is floating point
      uint mipOffset = scene2DTextures[ index ].Load( uint3( 0, 0, noLevels - 1 ) ).x;
      return scene2DMipTailTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, mipLevel - half( mipOffset ) );
    }
  
    uint2 texSizeLo = max( texSizeHi / 2, 1 );

    half4 sampleHi = 0;
    half4 sampleLo = 0;
    bool hasHi, hasLo;

    hasHi = TrySampleTextureLevel( index, mipHi, tc, texSizeHi, noLevels, sampleHi );
    if ( mipHi == mipLo )
    {
      // This is a special case, when we render mip zero. No need to sample it twice.
      hasLo    = hasHi;
      sampleLo = sampleHi;
    }
    else
      hasLo = TrySampleTextureLevel( index, mipLo, tc, texSizeLo, noLevels, sampleLo );
  
    if ( hasHi != hasLo )
      return hasHi ? sampleHi : sampleLo;
    if ( hasHi && hasLo )
      return lerp( sampleHi, sampleLo, frac( mipLevel ) );
  
    while ( !hasLo )
    {
      ++mipLo;
      texSizeLo = max( texSizeLo / 2, 1 );

      hasLo = TrySampleTextureLevel( index, mipLo, tc, texSizeLo, noLevels, sampleLo );
    }
  
    return sampleLo;
  #else
    return scene2DTextures[ index ].SampleLevel( anisotropicWrapSampler, tc, mipLevel );
  #endif
}

half4 SampleTexture( uint index, float2 tc, half mipLevel, uint2 screenPosition, uint feedbackPhase )
{
  #if ENABLE_TEXTURE_STREAMING && ENABLE_TEXTURE_FEEDBACK
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