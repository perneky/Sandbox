#include "../../ShaderValues.h"

#define _RootSignature "RootFlags( 0 )," \
                       "RootConstants( b0, num32BitConstants = 2 )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( UAV( u0 ) )" \

cbuffer cb0 : register( b0 )
{
  float2 inverseDimensions;
}

Texture2D< float3 >   InputBuf : register( t0 );
RWTexture2D< float3 > Result   : register( u0 );

static const float weights[ 5 ] = { 70.0f / 256.0f, 56.0f / 256.0f, 28.0f / 256.0f, 8.0f / 256.0f, 1.0f / 256.0f };

float3 BlurPixels( float3 a, float3 b, float3 c, float3 d, float3 e, float3 f, float3 g, float3 h, float3 i )
{
  return weights[ 0 ] * e + weights[ 1 ] * ( d + f ) + weights[ 2 ] * ( c + g ) + weights[ 3 ] * ( b + h ) + weights[ 4 ] * ( a + i );
}

groupshared uint CacheR[ 128 ];
groupshared uint CacheG[ 128 ];
groupshared uint CacheB[ 128 ];

void Store2Pixels( uint index, float3 pixel1, float3 pixel2 )
{
  CacheR[ index ] = f32tof16( pixel1.r ) | f32tof16( pixel2.r ) << 16;
  CacheG[ index ] = f32tof16( pixel1.g ) | f32tof16( pixel2.g ) << 16;
  CacheB[ index ] = f32tof16( pixel1.b ) | f32tof16( pixel2.b ) << 16;
}

void Load2Pixels( uint index, out float3 pixel1, out float3 pixel2 )
{
  uint rr = CacheR[ index ];
  uint gg = CacheG[ index ];
  uint bb = CacheB[ index ];
  pixel1 = float3( f16tof32( rr ), f16tof32( gg ), f16tof32( bb ) );
  pixel2 = float3( f16tof32( rr >> 16 ), f16tof32( gg >> 16 ), f16tof32( bb >> 16 ) );
}

void Store1Pixel( uint index, float3 pixel )
{
  CacheR[ index ] = asuint( pixel.r );
  CacheG[ index ] = asuint( pixel.g );
  CacheB[ index ] = asuint( pixel.b );
}

void Load1Pixel( uint index, out float3 pixel )
{
  pixel = asfloat( uint3( CacheR[ index ], CacheG[ index ], CacheB[ index ] ) );
}

void BlurHorizontally( uint outIndex, uint leftMostIndex )
{
  float3 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9;
  Load2Pixels( leftMostIndex + 0, s0, s1 );
  Load2Pixels( leftMostIndex + 1, s2, s3 );
  Load2Pixels( leftMostIndex + 2, s4, s5 );
  Load2Pixels( leftMostIndex + 3, s6, s7 );
  Load2Pixels( leftMostIndex + 4, s8, s9 );

  Store1Pixel( outIndex, BlurPixels( s0, s1, s2, s3, s4, s5, s6, s7, s8 ) );
  Store1Pixel( outIndex + 1, BlurPixels( s1, s2, s3, s4, s5, s6, s7, s8, s9 ) );
}

void BlurVertically( uint2 pixelCoord, uint topMostIndex )
{
  float3 s0, s1, s2, s3, s4, s5, s6, s7, s8;
  Load1Pixel( topMostIndex, s0 );
  Load1Pixel( topMostIndex + 8, s1 );
  Load1Pixel( topMostIndex + 16, s2 );
  Load1Pixel( topMostIndex + 24, s3 );
  Load1Pixel( topMostIndex + 32, s4 );
  Load1Pixel( topMostIndex + 40, s5 );
  Load1Pixel( topMostIndex + 48, s6 );
  Load1Pixel( topMostIndex + 56, s7 );
  Load1Pixel( topMostIndex + 64, s8 );

  Result[ pixelCoord ] = BlurPixels( s0, s1, s2, s3, s4, s5, s6, s7, s8 );
}

[ RootSignature( _RootSignature ) ]
[numthreads( BlurBloomKernelWidth, BlurBloomKernelHeight, 1 )]
void main( uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID )
{
  uint2 GroupUL = ( Gid.xy << 3 ) - 4;
  uint2 ThreadUL = ( GTid.xy << 1 ) + GroupUL;

  uint destIdx = GTid.x + ( GTid.y << 4 );
  Store2Pixels( destIdx + 0, InputBuf[ ThreadUL + uint2( 0, 0 ) ], InputBuf[ ThreadUL + uint2( 1, 0 ) ] );
  Store2Pixels( destIdx + 8, InputBuf[ ThreadUL + uint2( 0, 1 ) ], InputBuf[ ThreadUL + uint2( 1, 1 ) ] );

  GroupMemoryBarrierWithGroupSync();

  uint row = GTid.y << 4;
  BlurHorizontally( row + ( GTid.x << 1 ), row + GTid.x + ( GTid.x & 4 ) );

  GroupMemoryBarrierWithGroupSync();

  BlurVertically( DTid.xy, ( GTid.y << 3 ) + GTid.x );
}
