#include "../../ShaderValues.h"
#include "Utils.hlsli"

#define _RootSignature "RootFlags( 0 )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1 ) )," \
                       "DescriptorTable( SRV( t2 ) )," \
                       "DescriptorTable( SRV( t3 ) )," \
                       "DescriptorTable( SRV( t4 ) )," \
                       "DescriptorTable( UAV( u0 ) )," \
                       "DescriptorTable( UAV( u1 ) )," \
                       "DescriptorTable( UAV( u2 ) )," \
                       "DescriptorTable( UAV( u3 ) )," \
                       "DescriptorTable( UAV( u4 ) )," \
                       "DescriptorTable( UAV( u5 ) )," \
                       "DescriptorTable( UAV( u6 ) )," \
                       "DescriptorTable( UAV( u7 ) )," \
                       "DescriptorTable( UAV( u8 ) )," \
                       "DescriptorTable( UAV( u9 ) )," \

StructuredBuffer< NodeSlot     > nodes                   : register( t0 );
StructuredBuffer< MeshSlot     > meshes                  : register( t1 );
StructuredBuffer< LightSlot    > lights                  : register( t2 );
StructuredBuffer< MaterialSlot > materials               : register( t3 );
ByteAddressBuffer                rootNodeChildrenIndices : register( t4 );

RWStructuredBuffer< IndirectRender > indirectOpaqueRender                    : register( u0 );
RWStructuredBuffer< IndirectRender > indirectOpaqueTwoSidedRender            : register( u1 );
RWStructuredBuffer< IndirectRender > indirectOpaqueAlphaTestedRender         : register( u2 );
RWStructuredBuffer< IndirectRender > indirectOpaqueTwoSidedAlphaTestedRender : register( u3 );
RWStructuredBuffer< IndirectRender > indirectTranslucentRender               : register( u4 );
RWStructuredBuffer< IndirectRender > indirectTranslucentTwoSidedRender       : register( u5 );
RWByteAddressBuffer                  instanceCount                           : register( u6 );
RWStructuredBuffer< FrameParams >    frameParams                             : register( u7 );
RWStructuredBuffer< LightParams >    frameLights                             : register( u8 );
RWStructuredBuffer< Sky         >    sky                                     : register( u9 );

float4x4 CameraMatrixToViewMatrix( float4x4 c )
{
  float4x4 t = transpose( c );
  float4x4 v = c;

  v._41 = -dot( t[ 3 ], t[ 0 ] );
  v._42 = -dot( t[ 3 ], t[ 1 ] );
  v._43 = -dot( t[ 3 ], t[ 2 ] );

  v._14 = 0.0;
  v._24 = 0.0;
  v._34 = 0.0;
  v._44 = 1.0;

  return transpose( v );
}

void WriteMesh( RWStructuredBuffer< IndirectRender > buffer, uint bufferIndex, uint index, uint meshSlot, float4x4 transform )
{
  buffer[ index ].worldTransform = transform;
  buffer[ index ].ibIndex        = meshes[ meshSlot ].ibIndex;
  buffer[ index ].vbIndex        = meshes[ meshSlot ].vbIndex;
  buffer[ index ].materialIndex  = meshes[ meshSlot ].materialIndex;
  buffer[ index ].modelId        = index | ( bufferIndex << 13 );
  buffer[ index ].randomValues   = meshes[ meshSlot ].randomValues;

  buffer[ index ].vertexCountPerInstance = meshes[ meshSlot ].indexCount;
  buffer[ index ].instanceCount          = 1;
  buffer[ index ].startVertexLocation    = 0;
  buffer[ index ].startInstanceLocation  = 0;
}

bool IsOBBVisible( float4x4 viewProjection, float4x4 nodeTransform, float4 center, float4 extents )
{
  float4x4 mvp = mul( viewProjection, nodeTransform );
  
  float4 corners[ 8 ] =
  {
    mul( mvp, center + float4(  extents.x,  extents.y,  extents.z, 0 ) ),
    mul( mvp, center + float4(  extents.x,  extents.y, -extents.z, 0 ) ),
    mul( mvp, center + float4(  extents.x, -extents.y,  extents.z, 0 ) ),
    mul( mvp, center + float4(  extents.x, -extents.y, -extents.z, 0 ) ),
    mul( mvp, center + float4( -extents.x,  extents.y,  extents.z, 0 ) ),
    mul( mvp, center + float4( -extents.x,  extents.y, -extents.z, 0 ) ),
    mul( mvp, center + float4( -extents.x, -extents.y,  extents.z, 0 ) ),
    mul( mvp, center + float4( -extents.x, -extents.y, -extents.z, 0 ) ),
  };
  
  [unroll]
  for ( int ix = 0; ix < 8; ++ix )
    corners[ ix ].xyz /= corners[ ix ].w;
  
  float minX = min( min( min( corners[ 0 ].x, corners[ 1 ].x ), min( corners[ 2 ].x, corners[ 3 ].x ) ), min( min( corners[ 4 ].x, corners[ 5 ].x ), min( corners[ 6 ].x, corners[ 7 ].x ) ) );
  float maxX = max( max( max( corners[ 0 ].x, corners[ 1 ].x ), max( corners[ 2 ].x, corners[ 3 ].x ) ), max( max( corners[ 4 ].x, corners[ 5 ].x ), max( corners[ 6 ].x, corners[ 7 ].x ) ) );
  float minY = min( min( min( corners[ 0 ].y, corners[ 1 ].y ), min( corners[ 2 ].y, corners[ 3 ].y ) ), min( min( corners[ 4 ].y, corners[ 5 ].y ), min( corners[ 6 ].y, corners[ 7 ].y ) ) );
  float maxY = max( max( max( corners[ 0 ].y, corners[ 1 ].y ), max( corners[ 2 ].y, corners[ 3 ].y ) ), max( max( corners[ 4 ].y, corners[ 5 ].y ), max( corners[ 6 ].y, corners[ 7 ].y ) ) );
  float minZ = min( min( min( corners[ 0 ].z, corners[ 1 ].z ), min( corners[ 2 ].z, corners[ 3 ].z ) ), min( min( corners[ 4 ].z, corners[ 5 ].z ), min( corners[ 6 ].z, corners[ 7 ].z ) ) );
  float maxZ = max( max( max( corners[ 0 ].z, corners[ 1 ].z ), max( corners[ 2 ].z, corners[ 3 ].z ) ), max( max( corners[ 4 ].z, corners[ 5 ].z ), max( corners[ 6 ].z, corners[ 7 ].z ) ) );
  
  return !( minX > 1 || minY > 1 || minZ > 1 || maxX < -1 || maxY < -1 || maxZ < 0 );
}

[RootSignature( _RootSignature )]
[numthreads( CullingKernelWidth, 1, 1 )]
void main( uint3 dispatchThreadID : SV_DispatchThreadID )
{
  if ( dispatchThreadID.x >= rootNodeChildrenIndices.Load( 0 ) )
    return;

  uint startNode = dispatchThreadID.x;
  
  // Culling by the last frame's view projection
  float4x4 cullTransform = frameParams[ 0 ].vpTransform;

  uint     localRoot        = rootNodeChildrenIndices.Load( ( startNode + 1 ) * 4 );
  uint     currentNode      = localRoot;
  float4x4 currentTransform = mul( nodes[ 0 ].worldTransform, nodes[ localRoot ].worldTransform ); // Root * first_child
  uint     stackPointer     = 0;
  uint     nodeStack[ 4 ];
  float4x4 transformStack[ 4 ];

  while ( true )
  {
    // Process light
    if ( nodes[ currentNode ].lightSlot != InvalidSlot )
    {
      LightSlot light = lights[ nodes[ currentNode ].lightSlot ];

      uint lightIndex;
      InterlockedAdd( frameParams[ 0 ].lightCount, 1, lightIndex );
      
      half3 lightDirection = half3( normalize( -currentTransform._12_22_32 ) );

      frameLights[ lightIndex ].origin        = currentTransform._14_24_34_44;
      frameLights[ lightIndex ].direction     = half4( lightDirection, 1 );
      frameLights[ lightIndex ].color         = light.color;
      frameLights[ lightIndex ].attenuation   = light.attenuation;
      frameLights[ lightIndex ].theta_phi     = light.theta_phi;
      frameLights[ lightIndex ].castShadow    = light.castShadow;
      frameLights[ lightIndex ].scatterShadow = light.scatterShadow;
      frameLights[ lightIndex ].type          = light.type;

      if ( light.type == LightType::Directional )
      {
        float inclination = 0.4855;
        float azimuth     = 0.25;

        float theta = 3.141592653589793238 * (inclination - 0.5);
        float phi   = 2 * 3.141592653589793238 * (azimuth - 0.5);
        float sunX  = cos( phi );
        float sunY  = sin( phi ) * sin( theta );
        float sunZ  = sin( phi ) * cos( theta );

        sky[ 0 ].sunPosition     = currentTransform._12_22_32_42;
        sky[ 0 ].rayleigh        = 1;
        sky[ 0 ].mieCoefficient  = 0.005;
        sky[ 0 ].mieDirectionalG = 0.7;
        sky[ 0 ].turbidity       = 0.3;
        sky[ 0 ].exposure        = 0.5;
      }
    }

    // Process meshes
    for ( uint meshSlot = nodes[ currentNode ].firstMeshSlot; meshSlot != InvalidSlot; meshSlot = meshes[ meshSlot ].nextSlotIndex )
    {
      if ( !IsOBBVisible( cullTransform, currentTransform, meshes[ meshSlot ].aabbCenter, meshes[ meshSlot ].aabbExtents ) )
        continue;

      bool isOpaque      = ( materials[ meshes[ meshSlot ].materialIndex ].flags & MaterialSlot::Translucent ) == 0;
      bool isTwoSided    = ( materials[ meshes[ meshSlot ].materialIndex ].flags & MaterialSlot::TwoSided ) != 0;
      bool isAlphaTested = ( materials[ meshes[ meshSlot ].materialIndex ].flags & MaterialSlot::AlphaTested ) != 0;

      [branch]
      if ( isOpaque )
      {
        uint indirectIndex;
        if ( isTwoSided )
        {
          if ( isAlphaTested )
          {
            instanceCount.InterlockedAdd( 12, 1, indirectIndex );
            WriteMesh( indirectOpaqueTwoSidedAlphaTestedRender, 3, indirectIndex, meshSlot, currentTransform );
          }
          else
          {
            instanceCount.InterlockedAdd( 4, 1, indirectIndex );
            WriteMesh( indirectOpaqueTwoSidedRender, 1, indirectIndex, meshSlot, currentTransform );
          }
        }
        else
        {
          if ( isAlphaTested )
          {
            instanceCount.InterlockedAdd( 8, 1, indirectIndex );
            WriteMesh( indirectOpaqueAlphaTestedRender, 2, indirectIndex, meshSlot, currentTransform );
          }
          else
          {
            instanceCount.InterlockedAdd( 0, 1, indirectIndex );
            WriteMesh( indirectOpaqueRender, 0, indirectIndex, meshSlot, currentTransform );
          }
        }
      }
      else
      {
        if ( isTwoSided )
        {
          uint indirectIndex;
          instanceCount.InterlockedAdd( 20, 1, indirectIndex );
          WriteMesh( indirectTranslucentTwoSidedRender, 5, indirectIndex, meshSlot, currentTransform );
        }
        else
        {
          uint indirectIndex;
          instanceCount.InterlockedAdd( 16, 1, indirectIndex );
          WriteMesh( indirectTranslucentRender, 4, indirectIndex, meshSlot, currentTransform );
        }
      }
    }

    if ( nodes[ currentNode ].firstChildSlot != InvalidSlot )
    {
      // There is a child node, jump to it
      nodeStack[ stackPointer ] = currentNode;
      transformStack[ stackPointer ] = currentTransform;
      ++stackPointer;

      currentNode = nodes[ currentNode ].firstChildSlot;
      currentTransform = mul( currentTransform, nodes[ currentNode ].worldTransform );
    }
    else if ( stackPointer != 0 && nodes[ currentNode ].nextSiblingSlot != InvalidSlot )
    {
      // For the local root, we don't check siblings. Those will be handled by the other threads.
      
      // There is no child, but a sibling, jump to that
      // This will fail if the root node has siblings, but that's not allowed
      currentNode = nodes[ currentNode ].nextSiblingSlot;
      currentTransform = mul( transformStack[ stackPointer - 1 ], nodes[ currentNode ].worldTransform );
    }
    else
    {
      // No child and no sibling, go back up the tree
      while( stackPointer > 0 )
      {
        --stackPointer;
        currentNode = nodeStack[ stackPointer ];
        currentTransform = transformStack[ stackPointer ];

        if ( stackPointer != 0 && nodes[ currentNode ].nextSiblingSlot != InvalidSlot )
        {
          // Again, for the local root, we don't check siblings. Those will be handled by the other threads.

          currentNode = nodes[ currentNode ].nextSiblingSlot;
          currentTransform = mul( transformStack[ stackPointer - 1 ], nodes[ currentNode ].worldTransform );
          break;
        }
      }

      if ( stackPointer == 0 )
        break;
    }
  }
}