#include "RootSignatures/ShaderStructures.hlsli"
#include "../../ShaderValues.h"
#include "Utils.hlsli"

#define _AERootSignature "RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS )," \
                         "DescriptorTable( CBV( b0 ) )," \
                         "DescriptorTable( SRV( t0 ) )," \
                         "DescriptorTable( SRV( t1 ) )," \
                         "DescriptorTable( SRV( t2 ) )," \
                         "DescriptorTable( SRV( t3 ) )," \
                         "DescriptorTable( SRV( t4 ) )," \
                         "DescriptorTable( SRV( t5 ) )," \
                         "DescriptorTable( SRV( t6, numDescriptors = " SceneBufferResourceCountStr  ", space = 6"  BigRangeFlags " ) )," \
                         "DescriptorTable( SRV( t7, numDescriptors = " SceneBufferResourceCountStr  ", space = 7"  BigRangeFlags " ) )," \
                         "DescriptorTable( SRV( t8, numDescriptors = " Scene2DResourceCountStr      ", space = 8"  BigRangeFlags " ) )," \
                         "DescriptorTable( SRV( t9, numDescriptors = 4,                                space = 9 ) )" \

cbuffer cb0 : register( b0 )
{
  FrameParams frameParams;
};

RaytracingAccelerationStructure rayTracingScene : register( t0 );

StructuredBuffer< MaterialSlot > materials : register( t1 );

StructuredBuffer < ModelMetaSlot > modelMetas : register( t2 );

Texture2D< float > depthTexture       : register( t3 );
Texture2D< half >  textureMipTexture  : register( t4 );
Texture2D< uint2 > geometryIdsTexture : register( t5 );

StructuredBuffer< uint >         meshIndices[]  : register( t6, space6 );
StructuredBuffer< VertexFormat > meshVertices[] : register( t7, space7 );

Texture2D< half4 > scene2DTextures[] : register( t8, space8 );

StructuredBuffer< IndirectRender > indirectBufers[] : register( t9, space9 );
