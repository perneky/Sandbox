#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "DescriptorTable( CBV( b0 ) )," \
                       "DescriptorTable( CBV( b1 ) )," \
                       "RootConstants( b2, num32BitConstants = 17 )," \

cbuffer cb0 : register( b0 )
{
  Sky sky;
};

cbuffer cb1 : register( b1 )
{
  FrameParams frameParams;
};

cbuffer cb2 : register( b2 )
{
  float4x4 viewProjTransform;
  uint     isCubeSide;
};

static const float sunAngularDiameterCos = 0.999956676946448443553574619906976478926848692873900859324;
static const float whiteScale            = 1.0748724675633854;

struct PixelIn3
{
  float4 position       : SV_POSITION;
  float4 vWorldPosition : POSITION;

  float3 vBetaR         : TEXCOORD0;
  float3 vBetaM         : TEXCOORD1;
  float3 vSunDirection  : POSITION2;
  float vSunE           : TEXCOORD3;
  float vSunfade        : TEXCOORD4;
  float mieDirectionalG : TEXCOORD5;
  float exposure        : TEXCOORD6;
};

