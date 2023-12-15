#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "RootConstants( b0, num32BitConstants = 22 )," \
                       "DescriptorTable( CBV( b1 ) )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1 ) )," \
                       "DescriptorTable( SRV( t2 ) )," \
                       "DescriptorTable( SRV( t3 ) )," \
                       "DescriptorTable( SRV( t4 ) )," \
                       "DescriptorTable( SRV( t5 ) )," \
                       "DescriptorTable( SRV( t6, numDescriptors = " SceneBufferResourceCountStr ", space = 6"  BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t7, numDescriptors = " SceneBufferResourceCountStr ", space = 7"  BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t8, numDescriptors = " Scene2DResourceCountStr     ", space = 8"  BigRangeFlags " ) )," \
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

cbuffer cb0 : register( b0 )
{
  float4x4 worldTransform;
  half4    randomValues;
  uint     ibIndex;
  uint     vbIndex;
  uint     materialIndex;
  uint     modelId;
};

cbuffer cb1 : register( b1 )
{
  FrameParams frameParams;
};

RaytracingAccelerationStructure rayTracingScene : register( t0 );

StructuredBuffer< MaterialSlot >  materials  : register( t1 );
StructuredBuffer< LightParams >   lights     : register( t2 );
StructuredBuffer< ModelMetaSlot > modelMetas : register( t3 );

Texture2D< half2 > brdfLUT : register( t4 );

TextureCube< half4 > skyCube : register( t5 );

StructuredBuffer< VertexFormat > meshVertices[] : register( t6, space6 );
StructuredBuffer< uint >         meshIndices[]  : register( t7, space7 );

Texture2D< half4 > scene2DTextures[] : register( t8, space8 );

#if TEXTURE_STREAMING_MODE != TEXTURE_STREAMING_OFF
  RWTexture2D< uint > scene2DTexturesFeedback[] : register( u0, space0 );
  RWByteAddressBuffer globalTextureFeedback     : register( u1, space1 );
#endif

SamplerState trilinearWrapSampler   : register( s0 );
SamplerState trilinearClampSampler  : register( s1 );
SamplerState anisotropicWrapSampler : register( s2 );

struct VertexOutput
{
  float4 screenPosition : SV_POSITION;
  half2  texcoord       : TEXCOORD;
  float3 worldPosition  : WORLD_POSITION;
  half4  worldNormal    : WORLD_NORMAL;
  half4  worldTangent   : WORLD_TANGENT;
  half4  worldBitangent : WORLD_BITANGENT;
};
