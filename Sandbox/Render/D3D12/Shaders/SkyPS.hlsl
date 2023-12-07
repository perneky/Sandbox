#define PS
#include "RootSignatures/Sky.hlsli"

float rayleighPhase( float cosTheta )
{
  const float THREE_OVER_SIXTEENPI = 0.05968310365946075;
  return THREE_OVER_SIXTEENPI * (1.0 + pow( cosTheta, 2.0 ));
}

float hgPhase( float cosTheta, float g )
{
  const float ONE_OVER_FOURPI = 0.07957747154594767;
  float g2 = pow( g, 2.0 );
  float inverse = 1.0 / pow( 1.0 - 2.0 * g * cosTheta + g2, 1.5 );
  return ONE_OVER_FOURPI * ((1.0 - g2) * inverse);
}

static const float A = 0.15;
static const float B = 0.50;
static const float C = 0.10;
static const float D = 0.20;
static const float E = 0.02;
static const float F = 0.30;

float3 Uncharted2Tonemap( float3 x )
{
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

[ RootSignature( _RootSignature ) ]
float4 main( PixelIn3 input ) : SV_TARGET
{
  const float rayleighZenithLength = 8.4E3;
  const float mieZenithLength = 1.25E3;
  const float3 up = float3(0.0, 1.0, 0.0);
  const float pi = 3.141592653589793238462643383279502884197169;

  float3 vBetaR_new = input.vBetaR;//float3(0.5, 0.5, 0.5);

  float3 direction = normalize( input.vWorldPosition.xyz );
  
  if ( direction.y < 0 )
    return 0.6;

  float zenithAngle = acos( max( 0.0, dot( up, direction ) ) );
  float inverse = 1.0 / (cos( zenithAngle ) + 0.15 * pow( 93.885 - ((zenithAngle * 180.0) / pi), -1.253 ));
  float sR = rayleighZenithLength * inverse;
  float sM = mieZenithLength * inverse;

  float3 Fex = exp( -(vBetaR_new * sR + input.vBetaM * sM) );

  float cosTheta = dot( direction, normalize( input.vSunDirection ) );

  float rPhase = rayleighPhase( cosTheta * 0.5 + 0.5 );

  float3 betaRTheta = vBetaR_new * rPhase;
  float mieDirectionalG_new = 0.7;
  //0.8
  float mPhase = hgPhase( cosTheta, input.mieDirectionalG );// Was: float mPhase = hgPhase( cosTheta, mieDirectionalG_new );
  float3 betaMTheta = input.vBetaM * mPhase;

  float3 Lin = pow( input.vSunE * ((betaRTheta + betaMTheta) / (vBetaR_new + input.vBetaM)) * (1.0 - Fex), float3(1.5, 1.5, 1.5) );
  Lin *= lerp( float3(1.0, 1.0, 1.0), pow( input.vSunE * ((betaRTheta + betaMTheta) / (vBetaR_new + input.vBetaM)) * Fex, float3(0.5, 0.5, 0.5) ), clamp( pow( 1.0 - dot( up, input.vSunDirection ), 5.0 ), 0.0, 1.0 ) );

  float theta = acos( direction.y );
  float phi = atan2( direction.z, direction.x );
  float2 uv = float2(phi, theta) / float2(2.0 * pi, pi) + float2(0.5, 0.0);
  float3 L0 = float3(0.1, 0.1, 0.1) * Fex;

  //Sun disk
  float sundisk = smoothstep( sunAngularDiameterCos, sunAngularDiameterCos + 0.00002, cosTheta );
  L0 += (input.vSunE * 19000.0 * Fex) * sundisk;

  float3 texColor = (Lin + L0) * 0.04 + float3(0.0, 0.0003, 0.00075);
  float f = 1.0 / (1.2 + (1.2 * sky.vSunfade));
  float3 retColor = pow( texColor * input.exposure, float3(f,f,f) );
  
  return float4 (retColor, 1.0);
}