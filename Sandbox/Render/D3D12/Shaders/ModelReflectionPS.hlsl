/*#define PS
#include "RootSignatures/Model.hlsli"
#include "Reflection.hlsli"

[ RootSignature( _RootSignature ) ]*/
float4 main(/* VertexOutput input */) : SV_Target0
{
  return 0;
  /*
  float3 worldNormal = normalize( input.worldNormal );

  float3 albedo    = materials[ materialIndex ].albedo;
  float  roughness = materials[ materialIndex ].roughness;
  float  metallic  = materials[ materialIndex ].metallic;

  float3 diffuseIBL       = 0;
  float3 specularIBL      = 0;
  float3 tracedReflection = 0;
  TraceIndirectLighting( 0, engine2DTextures[ SpecBRDFLUTSlot ], albedo, roughness, metallic, input.worldPosition, worldNormal, frameParams, diffuseIBL, specularIBL );

  [branch]
  if ( any( specularIBL > 0 ) )
    tracedReflection = CalcReflection( input.worldPosition, worldNormal, frameParams );

  return float4( tracedReflection, roughness );*/
}
