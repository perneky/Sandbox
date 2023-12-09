#define PS
#include "RootSignatures/ModelTranslucent.hlsli"
#include "Lighting.hlsli"
#include "TextureSampling.hlsli"
#include "CalcSurfaceNormal.hlsli"
#include "RTUtils.hlsli"

static const uint QueryFlags = RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;

half3 CalcReflection( float3 worldPosition, half3 worldNormal )
{
  half3 toCamera     = half3( normalize( frameParams.cameraPosition.xyz - worldPosition ) );
  half3 toReflection = reflect( -toCamera, worldNormal );

  RayQuery< QueryFlags > query;
  RayDesc                ray;

  ray.Origin    = worldPosition;
  ray.Direction = toReflection;
  ray.TMin      = 0.001;
  ray.TMax      = INF;

  query.TraceRayInline( rayTracingScene, QueryFlags, 0xFF, ray );
  query.Proceed();
  
  [branch]
  if ( query.CommittedStatus() == COMMITTED_TRIANGLE_HIT )
  {
    HitGeometry hitGeom = CalcClosestHitGeometryInline( query );

    MaterialSlot material = materials[ hitGeom.materialIndex ];

    half3 surfaceWorldNormal = hitGeom.worldNormal;
    half3 surfaceAlbedo      = material.albedo.rgb;
    half3 surfaceEmissive    = material.emissive.rgb;
    half  surfaceRoughness   = material.roughness_metallic.x;
    half  surfaceMetallic    = material.roughness_metallic.y;

    half3 probeGI         = 0.5;
    half3 effectiveAlbedo = surfaceAlbedo;
    half3 directLighting  = TraceDirectLighting( effectiveAlbedo, surfaceRoughness, surfaceMetallic, hitGeom.worldPosition, surfaceWorldNormal, 1, worldPosition, frameParams.lightCount );

    half3 diffuseIBL;
    half3 specularIBL;
    TraceIndirectLighting( probeGI
                         , brdfLUT
                         , effectiveAlbedo
                         , surfaceRoughness
                         , surfaceMetallic
                         , hitGeom.worldPosition
                         , surfaceWorldNormal
                         , worldPosition
                         , diffuseIBL
                         , specularIBL );
  
    return surfaceEmissive + directLighting + diffuseIBL;
  }
  else
  {
    return skyCube.SampleLevel( trilinearWrapSampler, toReflection, 0 ).rgb;
  }
}

[ RootSignature( _RootSignature ) ]
float4 main( VertexOutput input, bool isFrontFace : SV_IsFrontFace ) : SV_Target0
{
  half3 worldNormal   = input.worldNormal.xyz * ( isFrontFace ? 1 : -1 );
  half3 surfaceNormal = CalcSurfaceNormal( materials[ materialIndex ].albedoTextureIndex, input.texcoord, input.worldNormal.xyz, input.worldTangent.xyz, input.worldBitangent.xyz, 0 );
  half3 albedo        = materials[ materialIndex ].albedo.rgb;
  half  alpha         = materials[ materialIndex ].albedo.a;
  half3 emissive      = materials[ materialIndex ].emissive.rgb;
  half  roughness     = materials[ materialIndex ].roughness_metallic.x;
  half  metallic      = materials[ materialIndex ].roughness_metallic.y;

  half textureMip = 0;
  int texIndex = ENABLE_TEXTURE_STREAMING ? materials[ materialIndex ].albedoTextureRefIndex : materials[ materialIndex ].albedoTextureIndex;
  if ( texIndex > -1 )
    #if ENABLE_TEXTURE_STREAMING
      textureMip = half( engine2DReferenceTextures[ texIndex ].CalculateLevelOfDetail( anisotropicWrapSampler, input.texcoord ) );
    #else
      textureMip = half( scene2DTextures[ texIndex ].CalculateLevelOfDetail( anisotropicWrapSampler, input.texcoord ) );
    #endif
  
  if ( materials[ materialIndex ].albedoTextureIndex >= 0 )
  {
    half4 sample = SampleTexture( materials[ materialIndex ].albedoTextureIndex, input.texcoord, textureMip, input.texcoord, frameParams.feedbackPhase );
    albedo = sample.rgb;
    alpha  = sample.a;
  }

  if ( materials[ materialIndex ].roughnessTextureIndex >= 0 )
    roughness = SampleTexture( materials[ materialIndex ].roughnessTextureIndex, input.texcoord, textureMip, input.texcoord, frameParams.feedbackPhase ).r;

  if ( materials[ materialIndex ].metallicTextureIndex >= 0 )
    metallic = SampleTexture( materials[ materialIndex ].metallicTextureIndex, input.texcoord, textureMip, input.texcoord, frameParams.feedbackPhase ).r;

  half3 probeGI = 0.5;

  half3 tracedDirectLighting = TraceDirectLighting( albedo, roughness, metallic, input.worldPosition, surfaceNormal, 1, frameParams.cameraPosition.xyz, frameParams.lightCount );

  half3 diffuseIBL        = 0;
  half3 specularIBL       = 0;
  half3 surfaceReflection = 0;
  TraceIndirectLighting( probeGI
                       , brdfLUT
                       , albedo
                       , roughness
                       , metallic
                       , input.worldPosition
                       , surfaceNormal
                       , frameParams.cameraPosition.xyz
                       , diffuseIBL
                       , specularIBL );

  [branch]
  if ( any( specularIBL > 0 ) )
    surfaceReflection = CalcReflection( input.worldPosition, surfaceNormal );

  half3 directLighting   = emissive + tracedDirectLighting + diffuseIBL;
  half3 combinedLighting = directLighting + surfaceReflection * specularIBL;

  return float4( combinedLighting, alpha );
}
