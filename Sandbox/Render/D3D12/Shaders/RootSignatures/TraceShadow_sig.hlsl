#include "ShaderStructures.hlsli"
#include "../../../ShaderValues.h"

#define _RootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                       "DescriptorTable( CBV( b0 ) )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1 ) )," \
                       "DescriptorTable( SRV( t2 ) )," \
                       "DescriptorTable( SRV( t3 ) )," \
                       "DescriptorTable( SRV( t4 ) )," \
                       "DescriptorTable( SRV( t10 ) )," \
                       "DescriptorTable( SRV( t11 ) )," \
                       "DescriptorTable( UAV( u0 ) )," \
                       "DescriptorTable( UAV( u1 ) )," \
                       "DescriptorTable( SRV( t5, numDescriptors = " SceneBufferResourceCountStr  ", space = 5" BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t6, numDescriptors = " SceneBufferResourceCountStr  ", space = 6" BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t7, numDescriptors = " Scene2DResourceCountStr      ", space = 7" BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t8, numDescriptors = " Scene2DResourceCountStr      ", space = 8" BigRangeFlags " ) )," \
                       "DescriptorTable( SRV( t9, numDescriptors = " Engine2DTileTexturesCountStr ", space = 9" BigRangeFlags " ) )," \
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

[ RootSignature( _RootSignature ) ]
[ numthreads( 1, 1, 1 ) ]
void main()
{
}