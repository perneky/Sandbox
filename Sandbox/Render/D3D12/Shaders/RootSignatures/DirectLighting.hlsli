#define ENABLE_TEXTURE_FEEDBACK 1

#include "../AttributeExtractor.hlsli"

#define _DLRootSignature "DescriptorTable( SRV( t11 ) )," \
                         "DescriptorTable( SRV( t12 ) )," \
                         "DescriptorTable( SRV( t13 ) )," \
                         "DescriptorTable( SRV( t14 ) )," \
                         "DescriptorTable( SRV( t15 ) )," \
                         "DescriptorTable( SRV( t16 ) )," \
                         "DescriptorTable( SRV( t17 ) )," \
                         "DescriptorTable( UAV( u1, space = 1 ) )," \
                         "DescriptorTable( UAV( u0, numDescriptors = " Scene2DResourceCountStr " ) )," \
                         "StaticSampler( s0," \
                         "               filter = FILTER_MIN_MAG_MIP_LINEAR," \
                         "               addressU = TEXTURE_ADDRESS_WRAP," \
                         "               addressV = TEXTURE_ADDRESS_WRAP," \
                         "               addressW = TEXTURE_ADDRESS_WRAP )," \
                         "StaticSampler( s1," \
                         "               filter = FILTER_MIN_MAG_MIP_LINEAR," \
                         "               addressU = TEXTURE_ADDRESS_CLAMP," \
                         "               addressV = TEXTURE_ADDRESS_CLAMP," \
                         "               addressW = TEXTURE_ADDRESS_CLAMP )," \
                         "StaticSampler( s2," \
                         "               filter        = FILTER_ANISOTROPIC," \
                         "               maxAnisotropy = 16," \
                         "               addressU      = TEXTURE_ADDRESS_WRAP," \
                         "               addressV      = TEXTURE_ADDRESS_WRAP," \
                         "               addressW      = TEXTURE_ADDRESS_WRAP )," \

StructuredBuffer< LightParams > lights : register( t11 );

Texture2D< half2 > brdfLUT : register( t12 );

TextureCube< half4 > skyCube : register( t13 );

Texture2D< half > aoTexture : register( t14 );
Texture2D< half4 > shadowTexture : register( t15 );

Texture2D< float4 > rtReflection : register( t16 );

Texture2D< float4 > giTexture : register( t17 );

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

#if TEXTURE_STREAMING_MODE != TEXTURE_STREAMING_OFF
  RWTexture2D< uint > scene2DTexturesFeedback[] : register( u0, space0 );
  RWByteAddressBuffer globalTextureFeedback     : register( u1, space1 );
#endif

struct VertexOutput
{
  float4 screenPosition : SV_POSITION;
};
