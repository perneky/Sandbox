#define PS
#include "RootSignatures/ModelDepth.hlsli"
#include "TextureSampling.hlsli"
#include "Utils.hlsli"

#ifndef DoAlphaTest
  #define DoAlphaTest()
#endif

float2 CalcScreenPosition( float4 clipPosition )
{
  float3 screenPosition = clipPosition.xyz / clipPosition.w;
  float2 screenTexcoord = screenPosition.xy * 0.5 + 0.5;
  screenTexcoord.y = 1.0 - screenTexcoord.y;
  return screenTexcoord * frameParams.rendererSizeF;
}

[ RootSignature( _RootSignature ) ]
PixelOutput main( VertexOutput input, bool isFrontFace : SV_IsFrontFace )
{
  PixelOutput output;
  
  bool isAlphaTested = false;
  
  if ( materials[ materialIndex ].albedoTextureIndex > -1 )
    output.textureMip = scene2DTextures[ materials[ materialIndex ].albedoTextureIndex ].CalculateLevelOfDetail( anisotropicWrapSampler, input.texcoord );
  else
    output.textureMip = 0;
  
  #ifdef ALPHA_TESTED_PASS
    isAlphaTested = ( materials[ materialIndex ].flags & MaterialSlot::AlphaTested ) != 0;
  
    [branch]
    if ( isAlphaTested )
      if ( SampleTexture( materials[ materialIndex ].albedoTextureIndex, input.texcoord, half( output.textureMip ) ).a < 0.5 )
        discard;
  #endif

  float2 posInTexels     = CalcScreenPosition( input.clipPosition );
  float2 prevPosInTexels = CalcScreenPosition( input.prevClipPosition );
  
  output.motionVector  = prevPosInTexels - posInTexels;
  output.geometryIds.x = modelId | ( isFrontFace ? 1 << 15 : 0 );
  output.geometryIds.y = input.triangleId;
  
  return output;
}
