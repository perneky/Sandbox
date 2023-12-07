#include "AttributeExtractor.hlsli"

cbuffer cb1 : register( b1 )
{
  float4 hitDistParams;
};

StructuredBuffer< LightParams > lights : register( t11 );

Texture2D< half2 > brdfLUT : register( t12 );
TextureCube< half4 > skyCube : register( t13 );

RWTexture2D< float4 > reflectionTexture : register( u0 );

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

#include "TextureSampling.hlsli"
#include "AlphaTestInstance.hlsli"
#include "GetAttributes.hlsli"
#include "CalcSurfaceNormal.hlsli"
#include "Lighting.hlsli"
#include "RTUtils.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRDEncoding.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRD.hlsli"

[ shader( "raygeneration" ) ]
void raygen()
{
  uint2 rayIndex    = DispatchRaysIndex().xy;
  uint2 outputIndex = rayIndex;

  rayIndex.x *= 2;

  bool evenFrame = ( frameParams.frameIndex & 1 ) == 0;
  bool evenLine  = ( rayIndex.y & 1 ) == 0;

  if ( evenFrame == evenLine )
    ++rayIndex.x;

  Attributes attribs;
  if ( !GetAttributes( rayIndex, frameParams.rendererSizeF, attribs ) )
    return;

  half3 worldGeometryNormal = normalize( attribs.worldNormal );
  half3 worldSurfaceNormal  = CalcSurfaceNormal( materials[ attribs.materialIndex ].normalTextureIndex, attribs.texcoord, attribs.worldNormal, attribs.worldTangent, attribs.worldBitangent, attribs.textureMip );
  half  roughness           = materials[ attribs.materialIndex ].roughness_metallic.x;

  if ( materials[ attribs.materialIndex ].roughnessTextureIndex >= 0 )
    roughness = SampleTexture( materials[ attribs.materialIndex ].roughnessTextureIndex, attribs.texcoord, attribs.textureMip ).r;
  
  half3 reflecionVector = reflect( half3( normalize( attribs.worldPosition - frameParams.cameraPosition.xyz ) ), worldSurfaceNormal );
  
  float seed   = outputIndex.x + outputIndex.y * 3.43121412313 + frac( 1.12345314312 * ( frameParams.frameIndex % MAX_ACCUM_DENOISE_FRAMES ) );
  half3 sample = CosineSampleHemisphere( reflecionVector, seed, roughness * roughness );
  float SoN    = dot( worldGeometryNormal, sample );
  
  // The ray points into the surface, lets mirror it to the surface
  if ( SoN < 0 )
  {
    half3 T = cross( worldGeometryNormal, sample );
    half3 B = cross( worldGeometryNormal, T );
    sample = reflect( sample, B );
  }
  
  RayDesc ray;
  ray.Origin    = attribs.worldPosition;
  ray.Direction = sample;
  ray.TMin      = 0.001;
  ray.TMax      = INF;

  ReflectionPayload payload = { 1.xxx, NRD_FP16_MAX };
  TraceRay( rayTracingScene, 0, 0xFF, 0, 0, 0, ray, payload );
  
  float  hitDist = REBLUR_FrontEnd_GetNormHitDist( payload.distance, attribs.viewZ, hitDistParams, roughness );
  float4 packedReflection = REBLUR_FrontEnd_PackRadianceAndNormHitDist( payload.color, hitDist );
  reflectionTexture[ outputIndex ] = packedReflection;
}

[ shader( "anyhit" ) ]
void anyHit( inout ReflectionPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  // All not alpha tested geomteries are opaque, and doesn't call the anyHit shader.
  
  float4 sample;
  
  [branch]
  if ( AlphaTestInstance( InstanceID(), attribs.barycentrics, sample ) )
    AcceptHitAndEndSearch();
  else
  {
    payload.color = lerp( payload.color, payload.color * sample.rgb, sample.a );
    IgnoreHit();
  }
}

[ shader( "miss" ) ]
void miss( inout ReflectionPayload payload )
{
  payload.color   *= min( skyCube.SampleLevel( trilinearWrapSampler, WorldRayDirection(), 0 ).rgb, 128 );
  payload.distance = NRD_FP16_MAX;
}

[ shader( "closesthit" ) ]
void closestHit( inout ReflectionPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  HitGeometry hit = CalcClosestHitGeometry( attribs.barycentrics );
  
  MaterialSlot material = materials[ hit.materialIndex ];

  half3 surfaceWorldNormal = normalize( hit.worldNormal );
  half3 surfaceAlbedo      = material.albedo.rgb;
  half3 surfaceEmissive    = 0;
  half  surfaceRoughness   = material.roughness_metallic.x;
  half  surfaceMetallic    = material.roughness_metallic.y;

  if ( materials[ hit.materialIndex ].albedoTextureIndex >= 0 )
    surfaceAlbedo = SampleTexture( materials[ hit.materialIndex ].albedoTextureIndex, hit.texcoord ).rgb;

  if ( materials[ hit.materialIndex ].roughnessTextureIndex >= 0 )
    surfaceRoughness = SampleTexture( materials[ hit.materialIndex ].roughnessTextureIndex, hit.texcoord ).r;

  if ( materials[ hit.materialIndex ].metallicTextureIndex >= 0 )
    surfaceMetallic = SampleTexture( materials[ hit.materialIndex ].metallicTextureIndex, hit.texcoord ).r;

  half3 probeGI        = 0.5;
  half3 directLighting = TraceDirectLighting( surfaceAlbedo, surfaceRoughness, surfaceMetallic, hit.worldPosition, surfaceWorldNormal, 1, WorldRayOrigin(), frameParams.lightCount );

  half3 diffuseIBL;
  half3 specularIBL;
  TraceIndirectLighting( probeGI
                       , brdfLUT
                       , surfaceAlbedo
                       , surfaceRoughness
                       , surfaceMetallic
                       , hit.worldPosition
                       , surfaceWorldNormal
                       , WorldRayOrigin()
                       , diffuseIBL
                       , specularIBL );
  payload.color   *= half3( surfaceEmissive + directLighting + diffuseIBL );
  payload.distance = RayTCurrent();
}
