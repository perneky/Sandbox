#pragma once

#include "../Window.h"

class WinAPIWindow final : public Window
{
  friend struct Window;

public:
  ~WinAPIWindow();

  bool IsValid() const override;

  HWND GetWindowHandle() const;

  bool ProcessMessages() override;

  int GetClientWidth() override;
  int GetClientHeight() override;

  void SetCaption( const wchar_t* caption ) override;

  void DearImGuiNewFrame() override;

  void SetCameraControlMode( bool enabled ) override;
  bool IsCameraControlMode() const override;

private:
  WinAPIWindow( int width, int height );

  LRESULT WindowProc( HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam );
  static LRESULT CALLBACK WindowProcStatic( HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam );

  HWND windowHandle = nullptr;

  bool isFullscreen = false;

  bool closeRequested = false;

  bool cameraControlMode = false;

  eastl::optional< POINT > lastMousePosition;
};