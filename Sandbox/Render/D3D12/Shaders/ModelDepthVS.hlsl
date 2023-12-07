#include "RootSignatures/ModelDepth.hlsli"
#include "Utils.hlsli"

[ RootSignature( _RootSignature ) ]
VertexOutput main( uint vertexId : SV_VertexID )
{
  VertexOutput output = (VertexOutput)0;
  
  bool windingFlipped = materials[ materialIndex ].flags & MaterialSlot::FlipWinding;
  if ( windingFlipped && vertexId % 3 == 0 )
    ++vertexId;
  else if ( windingFlipped && vertexId % 3 == 1 )
    --vertexId;

  uint         index  = indexBuffers[ ibIndex ][ vertexId ];
  VertexFormat vertex = vertexBuffers[ vbIndex ][ index ];

  float3 worldPosition = mul( worldTransform, vertex.position ).xyz;

  output.screenPosition = mul( frameParams.vpTransform, float4( worldPosition, 1 ) );
  
  // This will break once we have dynamic objects, as world will be different for each frame
  output.clipPosition     = mul( frameParams.vpTransformNoJitter,     float4( worldPosition, 1 ) );
  output.prevClipPosition = mul( frameParams.prevVPTransformNoJitter, float4( worldPosition, 1 ) );
  output.texcoord         = vertex.texcoord;
  output.triangleId       = vertexId / 3;

  return output;
}