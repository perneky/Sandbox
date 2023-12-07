#define PS
#include "RootSignatures/DirectLighting.hlsli"
#include "Lighting.hlsli"
#include "TextureSampling.hlsli"
#include "GetAttributes.hlsli"
#include "CalcSurfaceNormal.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRDEncoding.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRD.hlsli"

[ RootSignature( _AERootSignature "," _DLRootSignature ) ]
half4 main( VertexOutput input ) : SV_Target0
{
  uint2  tci = input.screenPosition.xy;
  float2 tcf = input.screenPosition.xy / frameParams.rendererSizeF;
  
  Attributes attribs;
  if ( !GetAttributes( tci, frameParams.rendererSizeF, attribs ) )
    return 0;
  
  half3 albedo    = materials[ attribs.materialIndex ].albedo.xyz;
  half3 emissive  = 0;
  half  roughness = materials[ attribs.materialIndex ].roughness_metallic.x;
  half  metallic  = materials[ attribs.materialIndex ].roughness_metallic.y;

  if ( materials[ attribs.materialIndex ].albedoTextureIndex >= 0 )
    albedo = SampleTexture( materials[ attribs.materialIndex ].albedoTextureIndex, attribs.texcoord, attribs.textureMip, tci, frameParams.feedbackPhase ).rgb;

  if ( materials[ attribs.materialIndex ].roughnessTextureIndex >= 0 )
    roughness = SampleTexture( materials[ attribs.materialIndex ].roughnessTextureIndex, attribs.texcoord, attribs.textureMip, tci, frameParams.feedbackPhase ).r;

  if ( materials[ attribs.materialIndex ].metallicTextureIndex >= 0 )
    metallic = SampleTexture( materials[ attribs.materialIndex ].metallicTextureIndex, attribs.texcoord, attribs.textureMip, tci, frameParams.feedbackPhase ).r;
  
  half3 worldSurfaceNormal = CalcSurfaceNormal( materials[ attribs.materialIndex ].normalTextureIndex, attribs.texcoord, attribs.worldNormal, attribs.worldTangent, attribs.worldBitangent, attribs.textureMip, tci, frameParams.feedbackPhase );

  half3 probeGI = 0.5;

  half  ao         = aoTexture[ input.screenPosition.xy ];
  half4 shadowData = SIGMA_BackEnd_UnpackShadow( shadowTexture[ input.screenPosition.xy ] );
  half3 shadow     = lerp( shadowData.yzw, 1.0, shadowData.x );

  ao = lerp( 0.3, 1.0, ao );
  
  half3 tracedDirectLighting = TraceDirectLighting( albedo, roughness, metallic, attribs.worldPosition, worldSurfaceNormal, shadow, frameParams.cameraPosition.xyz, frameParams.lightCount );

  half3 diffuseIBL        = 0;
  half3 specularIBL       = 0;
  half3 surfaceReflection = 0;
  TraceIndirectLighting( probeGI
                       , brdfLUT
                       , albedo
                       , roughness
                       , metallic
                       , attribs.worldPosition
                       , worldSurfaceNormal
                       , frameParams.cameraPosition.xyz
                       , diffuseIBL
                       , specularIBL );
  diffuseIBL  *= ao;
  specularIBL *= ao;

  [branch]
  if ( any( specularIBL > 0 ) )
  {
    float4 reflectionTexel = rtReflection[ input.screenPosition.xy ];
    surfaceReflection = half3( REBLUR_BackEnd_UnpackRadianceAndNormHitDist( reflectionTexel ).rgb );
  }

  half3 directLighting   = emissive + tracedDirectLighting + diffuseIBL;
  half3 combinedLighting = directLighting + surfaceReflection * specularIBL;

  return half4( combinedLighting, 1 );
}
