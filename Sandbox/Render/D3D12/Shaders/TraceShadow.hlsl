#include "RootSignatures/ShaderStructures.hlsli"
#include "../../ShaderValues.h"

cbuffer cb0 : register( b0 )
{
  FrameParams frameParams;
};

RaytracingAccelerationStructure rayTracingScene : register( t0 );

StructuredBuffer < ModelMetaSlot > modelMetas : register( t1 );

StructuredBuffer< MaterialSlot > materials : register( t2 );
StructuredBuffer< LightParams >  lights    : register( t3 );

Texture2D< float > depthTexture : register( t4 );

StructuredBuffer< uint >         meshIndices[]  : register( t5, space5 );
StructuredBuffer< VertexFormat > meshVertices[] : register( t6, space6 );

#if ENABLE_TEXTURE_STREAMING
  Texture2D< uint2 > scene2DTextures[]        : register( t7, space7 );
  Texture2D< half4 > scene2DMipTailTextures[] : register( t8, space8 );
  Texture2D< half4 > engine2DTileTextures[]   : register( t9, space9 );
#else
  Texture2D< half4 > scene2DTextures[] : register( t7, space7 );
#endif

Texture2D< uint3 > scramblingRankingTexture : register( t10 );
Texture2D< uint4 > sobolTexture             : register( t11 );

RWTexture2D< float2 > shadowTexture      : register( u0 );
RWTexture2D< float4 > shadowTransTexture : register( u1 );

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

#include "AlphaTestInstance.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRDEncoding.hlsli"
#include "../../../../External/NRD/Shaders/Include/NRD.hlsli"

static const float solViewRadialAngle    = 0.0093f / 2;
static const float solViewTanRadialAngle = tan( solViewRadialAngle );

uint ReverseBits4( uint x )
{
  x = ( ( x & 0x5 ) << 1 ) | ( ( x & 0xA ) >> 1 );
  x = ( ( x & 0x3 ) << 2 ) | ( ( x & 0xC ) >> 2 );

  return x;
}

uint Bayer4x4ui( uint2 samplePos, uint frameIndex, const uint mode = 1 )
{
  uint2 samplePosWrap = samplePos & 3;
  uint a = 2068378560 * ( 1 - ( samplePosWrap.x >> 1 ) ) + 1500172770 * ( samplePosWrap.x >> 1 );
  uint b = ( samplePosWrap.y + ( ( samplePosWrap.x & 1 ) << 2 ) ) << 2;

  uint sampleOffset = mode == 1 ? ReverseBits4( frameIndex ) : frameIndex;

  return ( ( a >> b ) + sampleOffset ) & 0xF;
}

float2 GetBlueNoise( uint2 pixelPos, bool isCheckerboard, uint seed, uint sampleIndex, uint sppVirtual = 1, uint spp = 1 )
{
  // Sample index
  uint frameIndex = isCheckerboard ? ( frameParams.frameIndex >> 1 ) : frameParams.frameIndex;
  uint virtualSampleIndex = ( frameIndex + seed ) & ( sppVirtual - 1 );
  sampleIndex &= spp - 1;
  sampleIndex += virtualSampleIndex * spp;

  // The algorithm
  uint3 A = scramblingRankingTexture[ pixelPos & 127 ];
  uint rankedSampleIndex = sampleIndex ^ A.z;
  uint4 B = sobolTexture[ uint2( rankedSampleIndex & 255, 0 ) ];
  float4 blue = ( float4( B ^ A.xyxy ) + 0.5 ) * ( 1.0 / 256.0 );

  // Randomize in [ 0; 1 / 256 ] area to get rid of possible banding
  uint d = Bayer4x4ui( pixelPos, frameParams.frameIndex );
  float2 dither = ( float2( d & 3, d >> 2 ) + 0.5 ) * ( 1.0 / 4.0 );
  blue += ( dither.xyxy - 0.5 ) * ( 1.0 / 256.0 );

  return saturate( blue.xy );
}

float3 GetRay( float2 rnd )
{
  float phi = rnd.x * 2 * PI;

  float cosTheta = sqrt( saturate( rnd.y ) );
  float sinTheta = sqrt( saturate( 1.0 - cosTheta * cosTheta ) );

  float3 ray;
  ray.x = sinTheta * cos( phi );
  ray.y = sinTheta * sin( phi );
  ray.z = cosTheta;

  return ray;
}

float3x3 GetBasis( float3 N )
{
  float sz = sign( N.z );
  float a  = 1.0 / ( sz + N.z );
  float ya = N.y * a;
  float b  = N.x * ya;
  float c  = N.x * sz;

  float3 T = float3( c * N.x * a - 1.0, sz * b, c );
  float3 B = float3( b, N.y * ya - sz, N.y );

  return float3x3( T, B, N );
}

[ shader( "raygeneration" ) ]
void raygen()
{
  uint2 rayIndex    = DispatchRaysIndex().xy;
  uint2 outputIndex = rayIndex;

  float rawDepth = depthTexture[ rayIndex ];

  [branch]
  if ( rawDepth >= 1 )
  {
    shadowTexture[ outputIndex ] = SIGMA_FrontEnd_PackShadow( INF, 0, 0 );
    return;
  }

  float4 clipDepth = float4( 0, 0, rawDepth, 1 );
  float4 viewDepth = mul( frameParams.invProjTransform, clipDepth );

  float4 ndcPos   = float4( ( ( rayIndex + 0.5 ) / frameParams.rendererSizeF ) * float2( 2, -2 ) + float2( -1, 1 ), rawDepth, 1 );
  float4 worldPos = mul( frameParams.invVpTransform, ndcPos );
  worldPos.xyz /= worldPos.w;

  float2 rnd = GetBlueNoise( rayIndex, false, 0, 0 );
  rnd  = GetRay( rnd ).xy;
  rnd *= solViewTanRadialAngle;

  float3x3 mSunBasis = GetBasis( -lights[ 0 ].direction.xyz );
  float3   sample    = normalize( mSunBasis[ 0 ] * rnd.x + mSunBasis[ 1 ] * rnd.y + mSunBasis[ 2 ] );

  RayDesc ray;
  ray.Origin    = worldPos.xyz;
  ray.Direction = sample;
  ray.TMin      = 0.001f;
  ray.TMax      = INF;

  ShadowPayload payload = { 1.xxx, 0 };
  TraceRay( rayTracingScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 0, 0, 0, ray, payload );

  float4 shadowTrans;
  shadowTexture[ outputIndex ] = SIGMA_FrontEnd_PackShadow( viewDepth.z / viewDepth.w, payload.distance, solViewTanRadialAngle, payload.color, shadowTrans );
  shadowTransTexture[ outputIndex ] = shadowTrans;
}

[ shader( "anyhit" ) ]
void anyHit( inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  // All not alpha tested geomteries are opaque, and doesn't call the anyHit shader.
  
  float4 sample;
  
  [branch]
  if ( AlphaTestInstance( InstanceID(), attribs.barycentrics, sample ) )
    AcceptHitAndEndSearch();
  else
  {
    payload.color    = lerp( payload.color, payload.color * sample.rgb, sample.a );
    payload.distance = max( payload.distance, RayTCurrent() );
    if ( any( payload.color > 0 ) )
      IgnoreHit();
    else
      AcceptHitAndEndSearch();
  }
}

[ shader( "miss" ) ]
void miss( inout ShadowPayload payload )
{
  payload.distance = payload.distance == 0 ? NRD_FP16_MAX : payload.distance;
}

[ shader( "closesthit" ) ]
void closestHit( inout ShadowPayload payload, in BuiltInTriangleIntersectionAttributes attribs )
{
  payload.color = 0;
  payload.distance = RayTCurrent();
}
