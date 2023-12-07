#include "Utils.hlsli"
#include "TextureSampling.hlsli"

bool AlphaTestInstance( uint instanceId, float2 barycentrics2, out float4 sample )
{
  ModelMetaSlot modelMeta = modelMetas[ instanceId ];

  int albedoTextureIndex = materials[ modelMeta.materialIndex ].albedoTextureIndex;
  if ( albedoTextureIndex < 0 )
    sample = materials[ modelMeta.materialIndex ].albedo.a;
  else
  {
    uint ibSlotIndex = modelMeta.indexBufferIndex;
    uint vbSlotIndex = modelMeta.vertexBufferIndex;

    StructuredBuffer< uint > meshIB  = meshIndices[ ibSlotIndex ];
    uint3                    indices = uint3( meshIB[ PrimitiveIndex() * 3 + 0 ]
                                            , meshIB[ PrimitiveIndex() * 3 + 1 ]
                                            , meshIB[ PrimitiveIndex() * 3 + 2 ] );

    bool windingFlipped = materials[ modelMeta.materialIndex ].flags & MaterialSlot::FlipWinding;
    if ( windingFlipped )
    {
      uint tmp = indices.x;
      indices.x = indices.y;
      indices.y = tmp;
    }
  
    half2 texcoords[ 3 ];

    VertexFormat v1 = meshVertices[ vbSlotIndex ][ indices.x ];
    VertexFormat v2 = meshVertices[ vbSlotIndex ][ indices.y ];
    VertexFormat v3 = meshVertices[ vbSlotIndex ][ indices.z ];
    texcoords[ 0 ] = v1.texcoord;
    texcoords[ 1 ] = v2.texcoord;
    texcoords[ 2 ] = v3.texcoord;

    half3 barycentrics = half3( 1.0 - barycentrics2.x - barycentrics2.y, barycentrics2.x, barycentrics2.y );

    half2 texcoord = texcoords[ 0 ] * barycentrics.x
                   + texcoords[ 1 ] * barycentrics.y
                   + texcoords[ 2 ] * barycentrics.z;

    sample = SampleTexture( albedoTextureIndex, texcoord );
  }

  half treshold = ( materials[ modelMeta.materialIndex ].flags & MaterialSlot::Translucent ) ? 1 : 0.5;
  return sample.a >= treshold;
}