#include "AttributeExtractor.hlsli"
#include "GetAttributes.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRDEncoding.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRD.hlsli"

#define _PDRootSignature "DescriptorTable( UAV( u0 ) )," \
                         "DescriptorTable( UAV( u1 ) )," \
                         "DescriptorTable( UAV( u2 ) )," \
                         "RootConstants( b1, num32BitConstants = 4 )," \
                         "StaticSampler( s0," \
                         "               filter = FILTER_MIN_MAG_MIP_LINEAR," \
                         "               addressU = TEXTURE_ADDRESS_WRAP," \
                         "               addressV = TEXTURE_ADDRESS_WRAP," \
                         "               addressW = TEXTURE_ADDRESS_WRAP )," \
                         "StaticSampler( s1," \
                         "               filter = FILTER_MIN_MAG_MIP_LINEAR," \
                         "               addressU = TEXTURE_ADDRESS_CLAMP," \
                         "               addressV = TEXTURE_ADDRESS_CLAMP," \
                         "               addressW = TEXTURE_ADDRESS_CLAMP )," \
                         "StaticSampler( s2," \
                         "               filter        = FILTER_ANISOTROPIC," \
                         "               maxAnisotropy = 16," \
                         "               addressU      = TEXTURE_ADDRESS_WRAP," \
                         "               addressV      = TEXTURE_ADDRESS_WRAP," \
                         "               addressW      = TEXTURE_ADDRESS_WRAP )," \

RWTexture2D< float4 > nrTexture       : register( u0 );
RWTexture2D< float  > viewZTexture    : register( u1 );
RWTexture2D< float4 > motion3DTexture : register( u2 );

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

cbuffer cb1 : register( b1 )
{
  uint width;
  uint height;
  float nearZ;
  float farZ;
};

#include "TextureSampling.hlsli"

[ RootSignature( _AERootSignature "," _PDRootSignature ) ]
[ numthreads( PrepareDenoiserKernelWidth, PrepareDenoiserKernelHeight, 1 ) ]
void main( uint3 dispatchThreadID : SV_DispatchThreadID )
{
  if ( dispatchThreadID.x >= width, dispatchThreadID.y >= height )
    return;
  
  float  viewZ;
  float4 normalRoughness;
  
  Attributes attribs;
  if ( GetAttributes( dispatchThreadID.xy, frameParams.rendererSizeF, attribs ) )
  {
    half roughness = materials[ attribs.materialIndex ].roughness_metallic.x;
    if ( materials[ attribs.materialIndex ].roughnessTextureIndex >= 0 )
      roughness = SampleTexture( materials[ attribs.materialIndex ].roughnessTextureIndex, attribs.texcoord, attribs.textureMip ).r;

    float  rawDepth  = depthTexture[ dispatchThreadID.xy ];
    float4 clipDepth = float4( 0, 0, rawDepth, 1 );
    float4 viewDepth = mul( frameParams.invProjTransform, clipDepth );

    viewZ           = viewDepth.z / viewDepth.w;
    normalRoughness = NRD_FrontEnd_PackNormalAndRoughness( normalize( attribs.worldNormal ), roughness );
  }
  else
  {
    viewZ           = 1000;
    normalRoughness = NRD_FrontEnd_PackNormalAndRoughness( 0, 0 );
  }

  viewZTexture[ dispatchThreadID.xy ] = viewZ;
  nrTexture[ dispatchThreadID.xy ] = normalRoughness;
  motion3DTexture[ dispatchThreadID.xy ] = 0;
}