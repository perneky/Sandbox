#pragma once

#include "Platform/Window.h"
#include "Render/Device.h"
#include "Render/RenderManager.h"
#include "Scene/Camera.h"
#include "Scene/Node.h"
#include "Scene/Scene.h"

namespace Sandbox
{
  eastl::vector< unsigned > windowSignalsForCamera;

  static constexpr float cameraRotationSpeed = 0.1f;

  float cameraMoveX      = 0;
  float cameraMoveY      = 0;
  float cameraMoveZ      = 0;
  int   cameraDX         = 0;
  int   cameraDY         = 0;
  bool  cameraFastMode   = false;
  bool  cameraSlowMode   = false;
  bool  toggleFullscreen = false;

  void SetupCameraControl( Window& window )
  {
    if ( !windowSignalsForCamera.empty() )
    {
      assert( windowSignalsForCamera.size() == 6 );

      window.SigKeyDown.Disconnect( windowSignalsForCamera[ 0 ] );
      window.SigKeyUp.Disconnect( windowSignalsForCamera[ 1 ] );
      window.SigMouseDown.Disconnect( windowSignalsForCamera[ 2 ] );
      window.SigMouseUp.Disconnect( windowSignalsForCamera[ 3 ] );
      window.SigMouseMoveBy.Disconnect( windowSignalsForCamera[ 4 ] );
      window.SigMouseMoveTo.Disconnect( windowSignalsForCamera[ 5 ] );

      windowSignalsForCamera.clear();
    }

    windowSignalsForCamera.emplace_back( window.SigKeyDown.Connect( []( int key )
    {
      switch ( key )
      {
      case 'W':        cameraMoveZ   += 1;    break;
      case 'S':        cameraMoveZ   -= 1;    break;
      case 'A':        cameraMoveX   -= 1;    break;
      case 'D':        cameraMoveX   += 1;    break;
      case 'Q':        cameraMoveY   += 1;    break;
      case 'E':        cameraMoveY   -= 1;    break;
      case VK_SHIFT:   cameraFastMode = true; break;
      case VK_CONTROL: cameraSlowMode = true; break;

      case VK_HOME:
        OutputDebugStringW( RenderManager::GetInstance().GetDevice().GetMemoryInfo( false ).data() );
        OutputDebugStringW( L"\n" );
        break;

      case VK_END:
        OutputDebugStringW( RenderManager::GetInstance().GetDevice().GetMemoryInfo( true ).data() );
        OutputDebugStringW( L"\n" );
        break;
      }
    } ) );
    windowSignalsForCamera.emplace_back( window.SigKeyUp.Connect( []( int key )
    {
      switch ( key )
      {
      case 'W':        cameraMoveZ   -= 1;     break;
      case 'S':        cameraMoveZ   += 1;     break;
      case 'A':        cameraMoveX   += 1;     break;
      case 'D':        cameraMoveX   -= 1;     break;
      case 'Q':        cameraMoveY   -= 1;     break;
      case 'E':        cameraMoveY   += 1;     break;
      case VK_SHIFT:   cameraFastMode = false; break;
      case VK_CONTROL: cameraSlowMode = false; break;

      case 'F': toggleFullscreen = true; break;
      }
    } ) );
    windowSignalsForCamera.emplace_back( window.SigMouseDown.Connect( [&]( int button, int x, int y )
    {
      if ( button == 1 )
        window.SetCameraControlMode( true );
    } ) );
    windowSignalsForCamera.emplace_back( window.SigMouseUp.Connect( [&]( int button )
    {
      if ( button == 1 )
        window.SetCameraControlMode( false );
    } ) );
    windowSignalsForCamera.emplace_back( window.SigMouseMoveBy.Connect( [&]( int dx, int dy )
    {
      if ( window.IsCameraControlMode() )
      {
        cameraDX = dx;
        cameraDY = dy;
      }
    } ) );
    windowSignalsForCamera.emplace_back( window.SigMouseMoveTo.Connect( [&]( int x, int y )
    {
    } ) );
  }

  void TickCamera( CommandList& commandList, Scene& scene, double timeElapsed )
  {
    if ( !cameraDX && !cameraDY && !cameraMoveX && !cameraMoveY && !cameraMoveZ )
      return;

    auto cameraNode    = scene.FindNodeByName( "Camera" );
    auto nodeTransform = cameraNode->GetFullTransform();

    if ( cameraDX )
    {
      auto yawAxis = g_XMIdentityR1;
      auto yaw     = XMMatrixRotationAxis( yawAxis, XMConvertToRadians( cameraDX * cameraRotationSpeed ) );
      
      nodeTransform.r[ 0 ]  = XMVector3TransformNormal( nodeTransform.r[ 0 ], yaw );
      nodeTransform.r[ 1 ]  = XMVector3TransformNormal( nodeTransform.r[ 1 ], yaw );
      nodeTransform.r[ 2 ]  = XMVector3TransformNormal( nodeTransform.r[ 2 ], yaw );

      cameraDX = 0;
    }

    if ( cameraDY )
    {
      auto pitchAxis = XMVector3Normalize( nodeTransform.r[ 0 ] );
      auto pitch     = XMMatrixRotationAxis( pitchAxis, XMConvertToRadians( cameraDY * cameraRotationSpeed ) );

      nodeTransform.r[ 0 ] = XMVector3TransformNormal( nodeTransform.r[ 0 ], pitch );
      nodeTransform.r[ 1 ] = XMVector3TransformNormal( nodeTransform.r[ 1 ], pitch );
      nodeTransform.r[ 2 ] = XMVector3TransformNormal( nodeTransform.r[ 2 ], pitch );

      cameraDY = 0;
    }

    if ( cameraMoveX || cameraMoveY || cameraMoveZ )
    {
      XMVECTOR offset = XMVectorSet( 0, 0, 0, 1 );
      if ( cameraMoveX != 0 )
      {
        auto t = float( cameraMoveX * timeElapsed );
        offset = XMVectorMultiplyAdd( nodeTransform.r[ 0 ], XMVectorSet(t, t, t, 1), offset);
      }
      if ( cameraMoveY != 0 )
      {
        auto t = float( cameraMoveY * timeElapsed );
        offset = XMVectorMultiplyAdd( nodeTransform.r[ 1 ], XMVectorSet( t, t, t, 1 ), offset );
      }
      if ( cameraMoveZ != 0 )
      {
        auto t = float( cameraMoveZ * timeElapsed );
        offset = XMVectorMultiplyAdd( nodeTransform.r[ 2 ], XMVectorSet( t, t, t, 1 ), offset );
      }
    
      if ( cameraFastMode )
        offset = XMVectorMultiply( offset, XMVectorSet( 10, 10, 10, 0 ) );
      else if ( cameraSlowMode )
        offset = XMVectorMultiply( offset, XMVectorSet( 0.2f, 0.2f, 0.2f, 0 ) );
      else
        offset = XMVectorMultiply( offset, XMVectorSet( 5, 5, 5, 0 ) );

      nodeTransform.r[ 3 ] += offset;
    }

    auto parentTransform    = cameraNode->GetParentFullTransform();
    auto invParentTransform = XMMatrixInverse( nullptr, parentTransform );
    auto localTransform     = nodeTransform * invParentTransform;

    cameraNode->SetTransform( localTransform );
    scene.OnNodeTransformChanged( commandList, *cameraNode );
  }
}
