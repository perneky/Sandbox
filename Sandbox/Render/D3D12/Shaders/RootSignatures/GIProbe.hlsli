#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "DescriptorTable( CBV( b0 ) )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1, offset = " Engine2DResourceBaseSlotStr ", numDescriptors = " Engine2DResourceCountStr ", space = 1 ) )," \
                       "StaticSampler( s0," \
                       "               maxAnisotropy = 1," \
                       "               filter        = FILTER_MIN_MAG_LINEAR_MIP_POINT," \
                       "               addressU      = TEXTURE_ADDRESS_WRAP," \
                       "               addressV      = TEXTURE_ADDRESS_WRAP," \
                       "               addressW      = TEXTURE_ADDRESS_WRAP )" \
                       ""

cbuffer cb0 : register( b0 )
{
  FrameParams frameParams;
};

Texture2DArray engine2DTextureArrays[] : register( t1, space1 );

SamplerState giSampler : register( s0 );

struct VertexInput
{
  float4 position : POSITION;
};

struct VertexOutput
{
  float3 worldPosition    : WORLD_POSITION;
  float3 worldNormal      : WORLD_NORMAL;
  float4 screenPosition   : SV_POSITION;
  
  nointerpolation uint probeId : PROBE_ID;
};
