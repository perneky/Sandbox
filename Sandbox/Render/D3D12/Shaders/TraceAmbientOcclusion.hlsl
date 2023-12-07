#include "AttributeExtractor.hlsli"

cbuffer cb1 : register( b1 )
{
  float4 hitDistParams;
};

RWTexture2D< half > aoTexture : register( u0 );

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

#include "AlphaTestInstance.hlsli"
#include "GetAttributes.hlsli"
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

  attribs.worldNormal = normalize( attribs.worldNormal );

  float seed   = outputIndex.x + outputIndex.y * 3.43121412313 + frac( 1.12345314312 * ( frameParams.frameIndex % MAX_ACCUM_DENOISE_FRAMES ) );
  half3 sample = CosineSampleHemisphere( attribs.worldNormal, seed );

  RayDesc ray;
  ray.Origin    = attribs.worldPosition;
  ray.Direction = sample;
  ray.TMin      = 0.001f;
  ray.TMax      = INF;

  AOPayload payload = { NRD_FP16_MAX };
  TraceRay( rayTracingScene, 0, 0xFF, 0, 0, 0, ray, payload );

  aoTexture[ outputIndex.xy ] = half( REBLUR_FrontEnd_GetNormHitDist( payload.t, 0, hitDistParams ) );
}

[ shader( "anyhit" ) ]
void anyHit( inout AOPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  // All not alpha tested or translucent geomteries are opaque, and doesn't call the anyHit shader.

  float4 sample;
  
  [branch]
  if ( AlphaTestInstance( InstanceID(), attribs.barycentrics, sample ) )
    AcceptHitAndEndSearch();
  else
  {
    payload.t = min( payload.t, RayTCurrent() / sample.a );
    IgnoreHit();
  }
}

[ shader( "miss" ) ]
void miss( inout AOPayload payload )
{
  payload.t = min( NRD_FP16_MAX, payload.t );
}

[ shader( "closesthit" ) ]
void closestHit( inout AOPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  payload.t = min( RayTCurrent(), payload.t );
}
