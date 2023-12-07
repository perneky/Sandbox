#define PS
#include "RootSignatures/ToneMapping.hlsli"

float3 TM_Reinhard( float3 hdr, float k = 1.0 )
{
  return hdr / (hdr + k);
}

float3 TM_Stanard( float3 hdr )
{
  return TM_Reinhard( hdr * sqrt( hdr ), sqrt( 4.0 / 27.0 ) );
}

[RootSignature( _RootSignature )]
float4 main( VertexOutput input ) : SV_Target0
{
  uint2  texId       = input.screenPosition.xy;
  float3 bloomSample = bloom.SampleLevel( linearSampler, input.screenPosition.xy * invTexSize, 0 ).rgb * bloomStrength;
  float3 hdrColor    = ( source[ texId ].rgb + bloomSample ) * exposure.exposure;
  float3 sdrColor    = TM_Stanard( hdrColor );
  return float4( sdrColor, 1.0 );
}