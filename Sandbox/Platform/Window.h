#pragma once

#include "Common/Signal.h"

struct Window
{
  static eastl::unique_ptr< Window > Create( int width, int height );

  virtual ~Window() = default;

  virtual bool IsValid() const = 0;

  virtual bool ProcessMessages() = 0;

  virtual int GetClientWidth() = 0;
  virtual int GetClientHeight() = 0;

  virtual void SetCaption( const wchar_t* caption ) = 0;

  virtual void DearImGuiNewFrame() = 0;

  virtual void SetCameraControlMode( bool enabled ) = 0;
  virtual bool IsCameraControlMode() const = 0;

  Signal< int >           SigKeyDown;
  Signal< int >           SigKeyUp;
  Signal< int, int, int > SigMouseDown;
  Signal< int >           SigMouseUp;
  Signal< int, int >      SigMouseMoveTo;
  Signal< int, int >      SigMouseMoveBy;

  Signal< int, int > SigSizeChanged;
};