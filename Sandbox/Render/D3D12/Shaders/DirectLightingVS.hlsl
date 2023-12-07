#include "RootSignatures/DirectLighting.hlsli"

static const float4 positions[] =
{
  float4( -1, -3, 0, 1 ),
  float4( -1,  1, 0, 1 ),
  float4(  3,  1, 0, 1 ),
};

[ RootSignature( _AERootSignature "," _DLRootSignature ) ]
VertexOutput main( uint vertexId : SV_VertexID )
{
  VertexOutput output = (VertexOutput)0;

  output.screenPosition = positions[ vertexId ];

  return output;
}
