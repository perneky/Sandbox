#pragma once

#include "../UIWindow.h"
#include "Render/Upscaling.h"
#include "Render/ShaderStructures.h"

enum class FrameDebugModeCB : int;

class DebugWindow : public UIWindow
{
public:
  DebugWindow();
  ~DebugWindow();

  void Tick( CommandList& commandList, double timeElapsed ) override;

  bool               GetUseVSync              () const;
  Upscaling::Quality GetUpscalingQuality      () const;
  bool               GetUpdateTextureStreaming() const;
  float              GetManualExposure        () const;
  int                GetDebugTextureIndex     () const;
  bool               GetFreezeCulling         () const;
  DebugOutput        GetDebugOutput           () const;


private:
  double   fpsAccum        = 0;
  uint64_t frameCounter    = 0;
  int      framesPerSecond = 0;

  bool               useVSync         = false;
  Upscaling::Quality upscalingQuality = Upscaling::DefaultQuality;

  bool freezeCulling = false;

  bool renderTexture      = false;
  int  renderTextureIndex = 0;

  bool updateTextureStreaming = true;

  float manualExposure = 1.1f;

  DebugOutput debugOutput = DebugOutput::None;
};