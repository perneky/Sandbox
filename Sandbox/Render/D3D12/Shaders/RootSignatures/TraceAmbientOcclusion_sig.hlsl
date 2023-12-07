#include "../AttributeExtractor.hlsli"

#define _TAORootSignature "DescriptorTable( UAV( u0 ) )," \
                          "RootConstants( b1, num32BitConstants = 4 )," \
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

[ RootSignature( _AERootSignature "," _TAORootSignature ) ]
[ numthreads( 1, 1, 1 ) ]
void main()
{
}