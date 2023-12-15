#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "RootConstants( b0, num32BitConstants = 22 )," \
                       "DescriptorTable( CBV( b1 ) )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1, numDescriptors = " SceneBufferResourceCountStr      ", space = 1" BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t2, numDescriptors = " SceneBufferResourceCountStr      ", space = 2" BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t3, numDescriptors = " Scene2DResourceCountStr          ", space = 3" BigRangeFlags " ) )," \
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

cbuffer cb0 : register( b0 )
{
  float4x4 worldTransform;
  half4    randomValues;
  uint     ibIndex;
  uint     vbIndex;
  uint     materialIndex;
  uint     modelId;
};

cbuffer cb1 : register( b1 )
{
  FrameParams frameParams;
};

StructuredBuffer< MaterialSlot > materials : register( t0 );

StructuredBuffer< VertexFormat > vertexBuffers[] : register( t1, space1 );
StructuredBuffer< uint >         indexBuffers[]  : register( t2, space2 );

Texture2D< half4 > scene2DTextures[] : register( t3, space3 );

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

struct VertexOutput
{
  float4 screenPosition   : SV_POSITION;
  float4 clipPosition     : CLIP_POSITION;
  float4 prevClipPosition : PREV_CLIP_POSITION;
  half2  texcoord         : TEXCOORD;

  nointerpolation uint triangleId : TRIANGLE_ID;
};

struct PixelOutput
{
  float2 motionVector  : SV_Target0;
  float  textureMip    : SV_Target1;
  uint2  geometryIds   : SV_Target2;
};