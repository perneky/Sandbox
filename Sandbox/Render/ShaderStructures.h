#pragma once

#ifndef SHADER_STRUCTURES_H
#define SHADER_STRUCTURES_H

#define matrix   XMFLOAT4X4 
#define float4x4 XMFLOAT4X4 
#define float4   XMFLOAT4   
#define float3   XMFLOAT3   
#define float2   XMFLOAT2   
#define int4     XMINT4     
#define int3     XMINT3     
#define int2     XMINT2     
#define uint4    XMUINT4    
#define uint3    XMUINT3    
#define uint2    XMUINT2    
#define half4    XMHALF4    
#define half2    XMHALF2    
#define uint     uint32_t   
#define const    constexpr  

#include "D3D12/Shaders/RootSignatures/ShaderStructures.hlsli"

#undef matrix
#undef float4
#undef float3
#undef float2
#undef int4
#undef int3
#undef int2
#undef uint4
#undef uint3
#undef uint2
#undef half4
#undef half2
#undef uint
#undef const

#endif