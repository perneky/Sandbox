#include "RootSignatures/Sky.hlsli"

float3 totalMie( float T )
{
  const float3 MieConst = float3(1.8399918514433978E14, 2.7798023919660528E14, 4.0790479543861094E14);
  float c = (0.2 * T) * 10E-18;
  return 0.434 * c * MieConst;
}

float sunIntensity( float zenithAngleCos )
{
  const float e = 2.71828182845904523536028747135266249775724709369995957;
  const float steepness = 1.5;
  const float cutoffAngle = 1.6110731556870734;
  zenithAngleCos = clamp( zenithAngleCos, -1.0, 1.0 );
  const float EE = 1000.0;
  return EE * max( 0.0, 1.0 - pow( e, -((cutoffAngle - acos( zenithAngleCos )) / steepness) ) );
}

static const float3 cubeVertices[] =
{
  float3( -1,  1, -1 ),
  float3(  1,  1, -1 ),
  float3( -1,  1,  1 ),
  float3(  1,  1,  1 ),
  float3( -1, -1, -1 ),
  float3(  1, -1, -1 ),
  float3( -1, -1,  1 ),
  float3(  1, -1,  1 ),
};

static const uint cubeIndices[] =
{
  0, 1, 2,  2, 1, 3,
  5, 4, 6,  6, 7, 5,

  0, 4, 1,  1, 4, 5,
  1, 5, 3,  5, 7, 3,

  0, 2, 6,  6, 4, 0,
  2, 3, 6,  3, 7, 6,
};

[ RootSignature( _RootSignature ) ]
PixelIn3 main( uint vertexId : SV_VertexID )
{
  const float3 totalRayleigh = float3(5.804542996261093E-6, 1.3562911419845635E-5, 3.0265902468824876E-5);

  float3 localPosition = cubeVertices[ cubeIndices[ vertexId ] ];

  float3   cameraPosition = isCubeSide ? 0                 : frameParams.cameraPosition.xyz;
  float4x4 viewProj       = isCubeSide ? viewProjTransform : frameParams.vpTransform;

  PixelIn3 output;
  output.vWorldPosition = float4( localPosition * 500 + cameraPosition, 1 );
  output.position       = mul( viewProj, output.vWorldPosition );

  #if USE_REVERSE_PROJECTION
    output.position.z = 0;
  #else
    output.position.z = output.position.w;
  #endif

  const float3 up = float3( 0.0, 1.0, 0.0 );
  
  output.vSunDirection = normalize( sky.sunPosition.xyz );
  output.vSunE         = sunIntensity( dot( output.vSunDirection, up ) );
  output.vSunfade      = 1.0 - clamp( 1.0 - exp( (sky.sunPosition.y / 450000.0) ), 0.0, 1.0 );

  float rayleighCoefficient = sky.rayleigh - (1.0 * (1.0 - output.vSunfade));
  
  output.vBetaR = totalRayleigh * rayleighCoefficient;
  output.vBetaM = totalMie( sky.turbidity ) * sky.mieCoefficient;

  output.mieDirectionalG = sky.mieDirectionalG;
  output.exposure        = sky.exposure;

  return output;
}
