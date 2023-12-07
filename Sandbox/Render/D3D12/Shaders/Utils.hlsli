#ifndef UTILS_H
#define UTILS_H

#include "RootSignatures/ShaderStructures.hlsli"

float4 PlaneFromPointNormal( float3 position, float3 normal )
{
  return float4( normal, -dot( position, normal ) );
}

float PointDistanceFromPlane( float3 position, float4 plane )
{
  return dot( position, plane.xyz ) + plane.w;
}

float3 FromsRGB( float3 c )
{
  return pow( c, 2.2 );
}

float3 GetNextRandomOffset( Texture3D randomTexture, SamplerState randomSampler, float3 randomOffset, inout float4 randomValues )
{
  static const float randomScale = 128;

  float3 offset = randomTexture.SampleLevel( randomSampler, frac( randomValues.xyz * randomScale + randomOffset ), 0 ).rgb * 2 - 1;
  randomValues.xyz += offset;
  return offset;
}

uint3 Load3x16BitIndices( ByteAddressBuffer indexBuffer, uint offsetBytes )
{
  uint3 indices;

  uint  dwordAlignedOffset = offsetBytes & ~3;
  uint2 four16BitIndices   = indexBuffer.Load2( dwordAlignedOffset );

  if ( dwordAlignedOffset == offsetBytes )
  {
    indices.x = four16BitIndices.x & 0xffff;
    indices.y = ( four16BitIndices.x >> 16 ) & 0xffff;
    indices.z = four16BitIndices.y & 0xffff;
  }
  else
  {
    indices.x = ( four16BitIndices.x >> 16 ) & 0xffff;
    indices.y = four16BitIndices.y & 0xffff;
    indices.z = ( four16BitIndices.y >> 16 ) & 0xffff;
  }

  return indices;
}

uint Load16BitIndex( ByteAddressBuffer indexBuffer, uint offsetBytes )
{
  uint dwordAlignedOffset = offsetBytes & ~3;
  uint two16BitIndices    = indexBuffer.Load( dwordAlignedOffset );

  if ( dwordAlignedOffset == offsetBytes )
    return two16BitIndices & 0xffff;
  else
    return ( two16BitIndices >> 16 ) & 0xffff;
}

float LinearizeDepth( float nonLinearDepth, float4x4 invProj )
{
  float4 ndcCoords  = float4( 0, 0, nonLinearDepth, 1.0f );
  float4 viewCoords = mul( invProj, ndcCoords );
  return viewCoords.z / viewCoords.w;
}

bool ShouldBeDiscared( float alpha )
{
  return alpha < 1.0 / 255;
}

void CalcAxesForVector( float3 v, out float3 a1, out float3 a2 )
{
  float3 axis;
  if ( abs( v.y ) < 0.9 )
    axis = float3( 0, 1, 0 );
  else
    axis = float3( 1, 0, 0 );

  a1 = cross( axis, v );
  a2 = cross( v, a1 );
}

float3 PertubNormal( float3 normal, float3 px, float3 py, float3 offset )
{
  return normalize( px * offset.x + py * offset.y + normal * ( abs( offset.z ) * 0.9 + 0.1 ) );
}

float3 PertubPosition( float3 position, float3 px, float3 py, float2 offset )
{
  return position + ( px * offset.x + py * offset.y );
}

float3 UVToDirectionSpherical( float2 uv )
{
  uv.x *= PI;
  uv.y *= PIPI;
  uv.y -= PI;

  float2 s, c;
  sincos( uv, s, c );

  float x = s.x * c.y;
  float y = s.x * s.y;
  float z = c.x;
  return float3( x, y, z );
}

float3 UVToDirectionOctahedral( float2 f )
{
  f = f * 2.0 - 1.0;

  // https://twitter.com/Stubbesaurus/status/937994790553227264
  float3 n = float3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
  float t = saturate( -n.z );
  n.x += n.x >= 0.0 ? -t : t;
  n.y += n.y >= 0.0 ? -t : t;
  return normalize( n );
}

float Lightness( float3 c )
{
  return dot( c, float3( 0.212671, 0.715160, 0.072169 ) );
}

float4x4 MatrixOrthographicLH( float ViewWidth, float ViewHeight, float NearZ, float FarZ )
{
  return float4x4(
      2.0f / ViewWidth, 0.0f, 0.0f, 0.0f,
      0.0f, 2.0f / ViewHeight, 0.0f, 0.0f,
      0.0f, 0.0f, 2.0f / (FarZ - NearZ), 0.0f,
      0.0f, 0.0f, (NearZ + FarZ) / (NearZ - FarZ), 1.0f
  );
}

float4x4 RotateX( float angle )
{
  float c = cos( angle );
  float s = sin( angle );
  return transpose( float4x4( 1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1 ) );
}

float4x4 Inverse( float4x4 m )
{
  float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
  float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
  float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
  float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

  float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
  float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
  float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
  float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

  float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
  float idet = 1.0f / det;

  float4x4 ret;

  ret[0][0] = t11 * idet;
  ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
  ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
  ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

  ret[1][0] = t12 * idet;
  ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
  ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
  ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

  ret[2][0] = t13 * idet;
  ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
  ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
  ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

  ret[3][0] = t14 * idet;
  ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
  ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
  ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

  return ret;
}

float3 CalcBaryCentrics( float3 loc, float3 wp1, float3 wp2, float3 wp3 )
{
  float3 v0    = wp2 - wp1;
  float3 v1    = wp3 - wp1;
  float3 v2    = loc - wp1;
  float  d00   = dot(v0, v0);
  float  d01   = dot(v0, v1);
  float  d11   = dot(v1, v1);
  float  d20   = dot(v2, v0);
  float  d21   = dot(v2, v1);
  float  denom = d00 * d11 - d01 * d01;
  float  v     = (d11 * d20 - d01 * d21) / denom;
  float  w     = (d00 * d21 - d01 * d20) / denom;
  float  u     = 1.0f - v - w;
  return float3( u, v, w );
}

half Hash1( inout float seed )
{
  return half( frac( sin( seed += 0.1 ) * 43758.5453123 ) );
}

half2 Hash2( inout float seed )
{
  float s1 = seed += 1;
  float s2 = seed += 1;
  return half2( frac( sin( float2( s1, s2 ) ) * float2( 43758.5453123,22578.1459123 ) ) );
}

half3 Hash3( inout float seed )
{
  float s1 = seed += 1;
  float s2 = seed += 1;
  float s3 = seed += 1;
  return half3( frac( sin( float3( s1, s2, s3 ) ) * float3( 43758.5453123, 22578.1459123, 19642.3490423 ) ) );
}

half3 UniformSampleHemisphere( half3 N, inout float seed )
{
  half2 u   = Hash2( seed );
  half  r   = half( sqrt( 1.0 - u.x * u.x ) );
  half  phi = half( 2.0 * PI * u.y );
  half3 B   = normalize( cross( N, half3( 0.0, 1.0, 1.0 ) ) );
  half3 T   = cross( B, N );
  return normalize( r * sin( phi ) * B + u.x * N + r * cos( phi ) * T );
}

half3 CosineSampleHemisphere( half3 n, inout float seed, half scale = 1 )
{
  half2 u     = Hash2( seed );
  half  r     = half( sqrt( u.x ) ) * scale;
  half  theta = half( 2.0 * PI * u.y );
  half3 B     = normalize( cross( n, half3( 0.0, 1.0, 1.0 ) ) );
  half3 T     = cross( B, n );
  return normalize( r * sin( theta ) * B + sqrt( 1.0 - u.x ) * n + r * cos( theta ) * T );
}

#endif