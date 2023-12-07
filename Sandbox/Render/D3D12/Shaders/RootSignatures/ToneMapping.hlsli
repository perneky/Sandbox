#include "../../../ShaderValues.h"
#include "ShaderStructures.hlsli"
#define _RootSignature "RootFlags( 0 )," \
                       "RootConstants( b0, num32BitConstants = 3 )," \
                       "DescriptorTable( CBV( b1 ) )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1 ) )," \
                       "StaticSampler( s0," \
                       "               filter = FILTER_MIN_MAG_MIP_LINEAR," \
                       "               addressU = TEXTURE_ADDRESS_CLAMP," \
                       "               addressV = TEXTURE_ADDRESS_CLAMP," \
                       "               addressW = TEXTURE_ADDRESS_CLAMP )"

cbuffer cb0 : register( b0 )
{
  float2 invTexSize;
  float  bloomStrength;
};

cbuffer cb1 : register( b1 )
{
  ExposureBuffer exposure;
}

Texture2D< float4 >   source      : register( t0 );
Texture2D< float4 >   bloom       : register( t1 );
RWTexture2D< float4 > destination : register( u0 );

SamplerState linearSampler : register( s0 );

struct VertexOutput
{
  float4 screenPosition : SV_POSITION;
};
