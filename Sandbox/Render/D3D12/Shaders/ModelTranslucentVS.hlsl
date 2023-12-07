#include "RootSignatures/ModelTranslucent.hlsli"
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

  uint         index  = meshIndices[ ibIndex ][ vertexId ];
  VertexFormat vertex = meshVertices[ vbIndex ][ index ];

  float3 worldPosition = mul( worldTransform, vertex.position ).xyz;

  output.screenPosition     = mul( frameParams.vpTransform, float4( worldPosition, 1 ) );
  output.texcoord           = vertex.texcoord;
  output.worldPosition      = worldPosition;
  output.worldNormal.xyz    = mul( (half3x3)worldTransform, vertex.normal.xyz );
  output.worldTangent.xyz   = mul( (half3x3)worldTransform, vertex.tangent.xyz );
  output.worldBitangent.xyz = mul( (half3x3)worldTransform, vertex.bitangent.xyz );

  return output;
}