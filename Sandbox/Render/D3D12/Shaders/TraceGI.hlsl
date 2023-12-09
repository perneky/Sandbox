#include "AttributeExtractor.hlsli"

cbuffer cb1 : register( b1 )
{
  float4 hitDistParams;
};

StructuredBuffer< LightParams > lights : register( t11 );

Texture2D< half2 > brdfLUT : register( t12 );
TextureCube< half4 > skyCube : register( t13 );

RWTexture2D< float4 > giTexture : register( u0 );

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

static const uint QueryFlags = RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;

half CalcShadow( float3 worldPosition )
{
  RayQuery< QueryFlags > query;
  RayDesc                ray;

  ray.Origin    = worldPosition;
  ray.Direction = -lights[ 0 ].direction.xyz;
  ray.TMin      = 0.001;
  ray.TMax      = INF;

  query.TraceRayInline( rayTracingScene, QueryFlags, 0xFF, ray );
  query.Proceed();
  
  if ( query.CommittedStatus() == COMMITTED_TRIANGLE_HIT )
    return 0;
  
  return 1;
}

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

  attribs.worldNormal = normalize( attribs.worldNormal );

  float seed   = outputIndex.x + outputIndex.y * 3.43121412313 + frac( 1.12345314312 * ( frameParams.frameIndex % MAX_ACCUM_DENOISE_FRAMES ) );
  half3 sample = CosineSampleHemisphere( attribs.worldNormal, seed );
  
  RayDesc ray;
  ray.Origin    = attribs.worldPosition;
  ray.Direction = sample;
  ray.TMin      = 0.001;
  ray.TMax      = INF;

  GIPayload payload = { 0.xxx, 0, 1, seed };
  TraceRay( rayTracingScene, 0, 0xFF, 0, 0, 0, ray, payload );
  
  float  hitDist  = REBLUR_FrontEnd_GetNormHitDist( payload.distance, attribs.viewZ, hitDistParams );
  float4 packedGI = REBLUR_FrontEnd_PackRadianceAndNormHitDist( payload.color, hitDist );
  giTexture[ outputIndex ] = packedGI;
}

[ shader( "anyhit" ) ]
void anyHit( inout GIPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
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
void miss( inout GIPayload payload )
{
  payload.color    = min( skyCube.SampleLevel( trilinearWrapSampler, WorldRayDirection(), 0 ).rgb, 128 );
  payload.distance = NRD_FP16_MAX;
}

[ shader( "closesthit" ) ]
void closestHit( inout GIPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  HitGeometry hit = CalcClosestHitGeometry( attribs.barycentrics );

  payload.distance += RayTCurrent();
  payload.seed     += RayTCurrent();

  half3 surfaceWorldNormal = normalize( hit.worldNormal );

  if ( payload.iteration < GI_MAX_ITERATIONS )
  {
    half3 sample = CosineSampleHemisphere( surfaceWorldNormal, payload.seed );

    RayDesc ray;
    ray.Origin    = hit.worldPosition;
    ray.Direction = sample;
    ray.TMin      = 0.001;
    ray.TMax      = INF;

    ++payload.iteration;
    
    TraceRay( rayTracingScene, 0, 0xFF, 0, 0, 0, ray, payload );
  }

  MaterialSlot material = materials[ hit.materialIndex ];

  half3 surfaceAlbedo    = material.albedo.rgb;
  half3 surfaceEmissive  = material.emissive.rgb;
  half  surfaceRoughness = material.roughness_metallic.x;
  half  surfaceMetallic  = material.roughness_metallic.y;

  if ( materials[ hit.materialIndex ].albedoTextureIndex >= 0 )
    surfaceAlbedo = SampleTexture( materials[ hit.materialIndex ].albedoTextureIndex, hit.texcoord ).rgb;

  if ( materials[ hit.materialIndex ].roughnessTextureIndex >= 0 )
    surfaceRoughness = SampleTexture( materials[ hit.materialIndex ].roughnessTextureIndex, hit.texcoord ).r;

  if ( materials[ hit.materialIndex ].metallicTextureIndex >= 0 )
    surfaceMetallic = SampleTexture( materials[ hit.materialIndex ].metallicTextureIndex, hit.texcoord ).r;

  half3 gi             = half3( payload.color );
  half  shadow         = CalcShadow( hit.worldPosition );
  half3 directLighting = TraceDirectLighting( surfaceAlbedo, surfaceRoughness, surfaceMetallic, hit.worldPosition, surfaceWorldNormal, shadow, WorldRayOrigin(), frameParams.lightCount );

  half3 diffuseIBL;
  half3 specularIBL;
  TraceIndirectLighting( gi
                       , brdfLUT
                       , surfaceAlbedo
                       , surfaceRoughness
                       , surfaceMetallic
                       , hit.worldPosition
                       , surfaceWorldNormal
                       , WorldRayOrigin()
                       , diffuseIBL
                       , specularIBL );
  payload.color = half3( surfaceEmissive + directLighting + diffuseIBL );
}
