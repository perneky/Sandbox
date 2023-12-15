#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "RootConstants( b0, num32BitConstants = 7 )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1, numDescriptors = " Scene2DResourceCountStr ", space = 1" BigRangeFlags " ) )," \
                       "StaticSampler( s0," \
                       "               filter        = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
                       "               addressU      = TEXTURE_ADDRESS_CLAMP," \
                       "               addressV      = TEXTURE_ADDRESS_CLAMP," \
                       "               addressW      = TEXTURE_ADDRESS_CLAMP )" \

cbuffer cb0 : register( b0 )
{
  float2      leftTop;
  float2      widthHeight;
  uint        mipLevel;
  uint        sceneTextureId;
  DebugOutput debugOutput;
};

Texture2D< half4 > texture        : register( t0 );
SamplerState       textureSampler : register( s0 );

Texture2D< half4 > scene2DTextures[] : register( t1, space1 );

struct VertexOutput
{
  float4 screenPosition : SV_POSITION;
  float2 texcoord       : TEXCOORD;
};
