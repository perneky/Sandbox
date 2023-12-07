#include "RootSignatures/ToneMapping.hlsli"

static const float4 vertices[] = { float4( -3, -1, 1, 1 ), float4(  1, -1, 1, 1 ), float4( 1, 3, 1, 1 ) };

[RootSignature( _RootSignature )]
VertexOutput main( uint vertexId : SV_VertexID )
{
  VertexOutput output = (VertexOutput)0;
  output.screenPosition = vertices[ vertexId ];
  return output;
}