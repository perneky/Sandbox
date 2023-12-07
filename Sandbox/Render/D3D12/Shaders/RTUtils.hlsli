#pragma once

#include "Utils.hlsli"

struct HitGeometry
{
  float3 worldPosition;
  half3  worldNormal;
  half2  texcoord;
  uint   materialIndex;
  float  t;
};

static const HitGeometry hitGeomMiss = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0 }, 0, -1 };

HitGeometry DoCalcHitGeometry( uint instanceId, uint triangleIndex, float2 barycentrics2, float hitDistance, float3x4 toWorld, float3 worldRayOrigin, float3 worldRayDirection )
{
  HitGeometry result;

  ModelMetaSlot modelMeta = modelMetas[ instanceId ];

  uint ibSlotIndex = modelMeta.indexBufferIndex;
  uint vbSlotIndex = modelMeta.vertexBufferIndex;

  StructuredBuffer< uint > meshIB  = meshIndices[ ibSlotIndex ];
  uint3                    indices = uint3( meshIB[ triangleIndex * 3 + 0 ]
                                          , meshIB[ triangleIndex * 3 + 1 ]
                                          , meshIB[ triangleIndex * 3 + 2 ] );

  bool windingFlipped = materials[ modelMeta.materialIndex ].flags & MaterialSlot::FlipWinding;
  if ( windingFlipped )
  {
    uint tmp = indices.x;
    indices.x = indices.y;
    indices.y = tmp;
  }
  
  half3 normals[ 3 ];
  half2 texcoords[ 3 ];

  VertexFormat v1 = meshVertices[ vbSlotIndex ][ indices.x ];
  VertexFormat v2 = meshVertices[ vbSlotIndex ][ indices.y ];
  VertexFormat v3 = meshVertices[ vbSlotIndex ][ indices.z ];
  normals[ 0 ] = v1.normal.xyz;
  normals[ 1 ] = v2.normal.xyz;
  normals[ 2 ] = v3.normal.xyz;
  texcoords[ 0 ] = v1.texcoord;
  texcoords[ 1 ] = v2.texcoord;
  texcoords[ 2 ] = v3.texcoord;

  half3 barycentrics = half3( 1.0 - barycentrics2.x - barycentrics2.y, barycentrics2.x, barycentrics2.y );

  half3 normal = normals[ 0 ] * barycentrics.x
               + normals[ 1 ] * barycentrics.y
               + normals[ 2 ] * barycentrics.z;

  half2 texcoord = texcoords[ 0 ] * barycentrics.x
                 + texcoords[ 1 ] * barycentrics.y
                 + texcoords[ 2 ] * barycentrics.z;
  
  result.worldPosition = worldRayOrigin + worldRayDirection * hitDistance;
  result.worldNormal   = mul( ( half3x3 )toWorld, normal );
  result.texcoord      = texcoord;
  result.materialIndex = modelMeta.materialIndex;
  result.t             = hitDistance;

  return result;
}

#define CalcClosestHitGeometry( barycentrics ) \
  DoCalcHitGeometry( InstanceID()              \
                   , PrimitiveIndex()          \
                   , barycentrics              \
                   , RayTCurrent()             \
                   , ObjectToWorld3x4()        \
                   , WorldRayOrigin()          \
                   , WorldRayDirection() )

#define CalcClosestHitGeometryInline( query )             \
  DoCalcHitGeometry( query.CommittedInstanceID()          \
                 , query.CommittedPrimitiveIndex()        \
                 , query.CommittedTriangleBarycentrics()  \
                 , query.CommittedRayT()                  \
                 , query.CommittedObjectToWorld3x4()      \
                 , query.WorldRayOrigin()                 \
                 , query.WorldRayDirection() )
