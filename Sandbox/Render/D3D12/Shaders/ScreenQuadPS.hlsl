#define PS
#include "RootSignatures/ScreenQuad.hlsli"

#include "../../../../External/NRD/Shaders/Include/NRDEncoding.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRD.hlsli"

[ RootSignature( _RootSignature ) ]
float4 main( VertexOutput input ) : SV_Target0
{
  bool   isMapped = false;
  float4 texel    = 0;

  if ( sceneTextureId < 0xFFFF )
  {
    #if TEXTURE_STREAMING_MODE == TEXTURE_STREAMING_RESERVED
      uint status;
      texel = scene2DTextures[ sceneTextureId ].SampleLevel( textureSampler, input.texcoord, mipLevel, 0, status );
      isMapped = CheckAccessFullyMapped( status );
    #else
      texel = scene2DTextures[ sceneTextureId ].SampleLevel( textureSampler, input.texcoord, mipLevel );
    #endif
  }
  else
  {
    switch ( debugOutput )
    {
      case DebugOutput::AO:
      case DebugOutput::Reflection:
      case DebugOutput::GI:
        texel = texture.SampleLevel( textureSampler, input.texcoord * float2( 0.5, 1 ), mipLevel );
        break;
      default: texel = texture.SampleLevel( textureSampler, input.texcoord, mipLevel );  break;
    }

    switch ( debugOutput )
    {
      case DebugOutput::AO:                 texel = float4( texel.rrr, 1 ); break;
      case DebugOutput::DenoisedAO:         texel = float4( texel.rrr, 1 ); break;
      case DebugOutput::Shadow:             texel = float4( texel.rrr, 1 ); break;
      case DebugOutput::DenoisedShadow:     texel = float4( lerp( SIGMA_BackEnd_UnpackShadow( texel ).yzw, 1.xxx, SIGMA_BackEnd_UnpackShadow( texel ).x ), 1 ); break;
      case DebugOutput::Reflection:         texel = float4( REBLUR_BackEnd_UnpackRadianceAndNormHitDist( texel ).rgb, 1 ); break;
      case DebugOutput::DenoisedReflection: texel = float4( REBLUR_BackEnd_UnpackRadianceAndNormHitDist( texel ).rgb, 1 ); break;
      case DebugOutput::GI:                 texel = float4( REBLUR_BackEnd_UnpackRadianceAndNormHitDist( texel ).rgb, 1 ); break;
      case DebugOutput::DenoisedGI:         texel = float4( REBLUR_BackEnd_UnpackRadianceAndNormHitDist( texel ).rgb, 1 ); break;
      default: break;
    }
    
    isMapped = true;
  }
  
  if ( !isMapped )
  {
    uint2 pos  = uint2( input.screenPosition.xy );
    uint2 rate = pos % 4;
    bool  red  = all( rate < 2 ) || all( rate > 1 );
    texel = red ? float4( 1, 0, 0, 1 ) : float4( 1, 1, 1, 1 );
  }
  
  if ( texel.a == 0 )
    discard;
  
  return texel;
}
