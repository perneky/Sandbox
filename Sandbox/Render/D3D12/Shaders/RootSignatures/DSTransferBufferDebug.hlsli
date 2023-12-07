#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "DescriptorTable( SRV( t0 ) )" \

ByteAddressBuffer transferBuffer : register( t0 );

struct VertexOutput
{
  float4 screenPosition : SV_POSITION;
};
