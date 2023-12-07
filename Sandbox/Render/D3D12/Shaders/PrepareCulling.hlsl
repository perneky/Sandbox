#include "../../ShaderValues.h"
#include "Utils.hlsli"

#define _RootSignature "RootFlags( 0 )," \
                       "RootConstants( b0, num32BitConstants = 40 )," \
                       "DescriptorTable( UAV( u0 ) )," \
                       "DescriptorTable( UAV( u1 ) )," \
                       "DescriptorTable( SRV( t0 ) )," \
                       "DescriptorTable( SRV( t1 ) )" \

cbuffer cb0 : register( b0 )
{
  float4x4 cameraViewProj;
  float4x4 cameraInvViewProj;

  float2 cameraJitter;
  uint2  rendererSize;
  uint   feedbackPhase;
  uint   frameIndex;
  
  uint freeze;

  uint cameraIndex;
};

RWByteAddressBuffer               instanceCount : register( u0 );
RWStructuredBuffer< FrameParams > frameParams   : register( u1 );

StructuredBuffer< NodeSlot > nodes : register( t0 );

StructuredBuffer< CameraSlot > cameras : register( t1 );

[RootSignature( _RootSignature )]
[numthreads( 1, 1, 1 )]
void main( uint3 dispatchThreadID : SV_DispatchThreadID )
{
  if ( !freeze )
  {
    instanceCount.Store( 0, 0 );
    instanceCount.Store( 4, 0 );
    instanceCount.Store( 8, 0 );
    instanceCount.Store( 12, 0 );
    instanceCount.Store( 16, 0 );
    instanceCount.Store( 20, 0 );

    frameParams[ 0 ].lightCount = 0;
  }

  frameParams[ 0 ].prevVPTransformNoJitter = frameParams[ 0 ].vpTransformNoJitter;
  frameParams[ 0 ].rendererSize            = rendererSize;
  frameParams[ 0 ].rendererSizeF           = rendererSize;
  frameParams[ 0 ].invRendererSize         = 1.0f / rendererSize;
  frameParams[ 0 ].feedbackPhase           = feedbackPhase;
  frameParams[ 0 ].frameIndex              = frameIndex;
  frameParams[ 0 ].frameIndexF             = frameIndex;
  
  AllMemoryBarrier();

  float4x4 currentTransform = mul( nodes[ 0 ].worldTransform, nodes[ cameraIndex ].worldTransform );

  if ( nodes[ cameraIndex ].cameraSlot != InvalidSlot )
  {
    CameraSlot camera = cameras[ nodes[ cameraIndex ].cameraSlot ];

    float4x4 cameraViewTransform = Inverse( currentTransform );
    float4x4 cameraProjTransform = camera.projTransform;

    float4x4 vpTransformNoJitter = mul( cameraProjTransform, cameraViewTransform );

    cameraProjTransform._13 += cameraJitter.x;
    cameraProjTransform._23 += cameraJitter.y;

    float4x4 vpTransform            = mul( cameraProjTransform, cameraViewTransform );
    float4x4 invVvpTransform        = Inverse( vpTransform );
    float4x4 invCameraProjTransform = Inverse( cameraProjTransform );

    frameParams[ 0 ].vpTransformNoJitter = vpTransformNoJitter;
    frameParams[ 0 ].viewTransform       = cameraViewTransform;
    frameParams[ 0 ].projTransform       = cameraProjTransform;
    frameParams[ 0 ].invProjTransform    = invCameraProjTransform;
    frameParams[ 0 ].cameraPosition      = currentTransform._14_24_34_44;
    frameParams[ 0 ].cameraDirection     = currentTransform._13_23_33_43;
    frameParams[ 0 ].vpTransform         = vpTransform;
    frameParams[ 0 ].invVpTransform      = invVvpTransform;
  }
  else
  {
    frameParams[ 0 ].vpTransform    = cameraViewProj;
    frameParams[ 0 ].invVpTransform = cameraInvViewProj;
  }
}