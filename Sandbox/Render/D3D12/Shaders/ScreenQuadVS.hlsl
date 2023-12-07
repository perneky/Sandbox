#include "RootSignatures/ScreenQuad.hlsli"

static const float2 corners[] =
{
  float2( 0, 0 ),
  float2( 0, 1 ),
  float2( 1, 0 ),
  float2( 1, 1 ),
};

[ RootSignature( _RootSignature ) ]
VertexOutput main( uint vertexId : SV_VertexID )
{
  VertexOutput output = (VertexOutput)0;
  
  float2 corner = corners[ vertexId ];
  float2 ndc    = leftTop + corner * widthHeight;

  output.screenPosition = float4( ndc, 0, 1 );
  output.texcoord       = corner;

  return output;
}