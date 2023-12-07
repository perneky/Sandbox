#include "RootSignatures/DSTransferBufferDebug.hlsli"

static const float2 corners[] =
{
  float2( -0.8,  0.8 ),
  float2( -0.8, -0.8 ),
  float2(  0.8,  0.8 ),
  float2(  0.8, -0.8 ),
};

[ RootSignature( _RootSignature ) ]
VertexOutput main( uint vertexId : SV_VertexID )
{
  VertexOutput output = (VertexOutput)0;
  
  output.screenPosition = float4( corners[ vertexId ], 0, 1 );

  return output;
}