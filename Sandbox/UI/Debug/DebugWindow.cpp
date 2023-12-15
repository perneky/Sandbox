#include "DebugWindow.h"
#include "../DearImGui/imgui.h"
#include "Render/ShaderStructures.h"
#include "Render/RenderManager.h"

static constexpr int fpsPeriod = 10;

static const char* GetUpscalingModeName( Upscaling::Quality quality )
{
  switch ( quality )
  {
  case Upscaling::Quality::Off:
    return "Off";
  case Upscaling::Quality::UltraQuality:
    return "Ultra quality";
  case Upscaling::Quality::Quality:
    return "Quality";
  case Upscaling::Quality::Balanced:
    return "Balanced";
  case Upscaling::Quality::Performance:
    return "Performance";
  case Upscaling::Quality::UltraPerformance:
    return "Ultra performance";
  default:
    assert( false );
    return " ";
  }
}

static const char* GetDebugOutputName( DebugOutput debugOutput )
{
  switch ( debugOutput )
  {
  case DebugOutput::None:
    return "None";
  case DebugOutput::Denoiser:
    return "Denoiser debug ouput";
  case DebugOutput::AO:
    return "Ambient occlusion";
  case DebugOutput::DenoisedAO:
    return "Denoised ambient occlusion";
  case DebugOutput::Shadow:
    return "Shadow";
  case DebugOutput::DenoisedShadow:
    return "Denoised shadow";
  case DebugOutput::Reflection:
    return "Reflection";
  case DebugOutput::DenoisedReflection:
    return "Denoised reflection";
  case DebugOutput::GI:
    return "Global illumination";
  case DebugOutput::DenoisedGI:
    return "Denoised global illumination";
  default:
    assert( false );
    return " ";
  }
}

DebugWindow::DebugWindow()
{
}

DebugWindow::~DebugWindow()
{
}

void DebugWindow::Tick( CommandList& commandList, double timeElapsed )
{
  fpsAccum += 1.0 / timeElapsed;
  if ( frameCounter++ % fpsPeriod == 0 )
  {
    wchar_t caption[ 100 ];
    framesPerSecond = int( fpsAccum / fpsPeriod );
    swprintf_s( caption, L"RTGame: %3d FPS", framesPerSecond );
    fpsAccum = 0;
  }

  auto memoryStats = RenderManager::GetInstance().GetMemoryStats();

  if ( ImGui::Begin( "Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) )
  {
    ImGui::Text( "Frames per second: %d", framesPerSecond );

    ImGui::Separator();

    ImGui::SliderFloat( "Manual exposure", &manualExposure, 0, 5 );

    ImGui::Checkbox( "Use VSync", &useVSync );

    if ( ImGui::BeginCombo( "Upscaling mode", GetUpscalingModeName( upscalingQuality ), ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightRegular ) )
    {
      #define FDM( m ) if ( ImGui::Selectable( GetUpscalingModeName( Upscaling::Quality::m ), upscalingQuality == Upscaling::Quality::m ) ) upscalingQuality = Upscaling::Quality::m;
        //FDM( UltraQuality );
        FDM( Off );
        FDM( Quality );
        FDM( Balanced );
        FDM( Performance );
        FDM( UltraPerformance );
      #undef FDM
      ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::Checkbox( "Freeze culling", &freezeCulling );

    ImGui::Separator();

    if ( ImGui::BeginCombo( "Debug output", GetDebugOutputName( debugOutput ), ImGuiComboFlags_PopupAlignLeft | ImGuiComboFlags_HeightRegular ) )
    {
      #define FDM( m ) if ( ImGui::Selectable( GetDebugOutputName( DebugOutput::m ), debugOutput == DebugOutput::m ) ) debugOutput = DebugOutput::m;
        FDM( None );
        FDM( Denoiser );
        #if USE_AO_WITH_GI
          FDM( AO );
          FDM( DenoisedAO );
        #endif // USE_AO_WITH_GI
        FDM( Shadow );
        FDM( DenoisedShadow );
        FDM( Reflection );
        FDM( DenoisedReflection );
        FDM( GI );
        FDM( DenoisedGI );
#undef FDM
      ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::Text( "Texture count: %d", memoryStats.textureCount );
    ImGui::Text( "Texture virtual allocation size (MB): %.3f", double( memoryStats.virtualAllocationSize ) / ( 1024 * 1024 ) );
    ImGui::Text( "Texture physical allocation size (MB): %.3f", double( memoryStats.physicalAllocationSize ) / ( 1024 * 1024 ) );

    if ( memoryStats.textureCount > 0 )
    {
      ImGui::Checkbox( "Update texture streaming", &updateTextureStreaming );

      ImGui::Checkbox( "Render texture", &renderTexture );

      if ( renderTexture )
        ImGui::SliderInt( "Texture index", &renderTextureIndex, 0, memoryStats.textureCount - 1 );

      ImGui::Checkbox( "Render heap texture", &renderHeapTexture );

      if ( renderHeapTexture )
        ImGui::SliderInt( "Heap texture index", &renderHeapTextureIndex, 0, 1 );
    }
  }
  ImGui::End();
}

bool DebugWindow::GetUseVSync() const
{
  return useVSync;
}

Upscaling::Quality DebugWindow::GetUpscalingQuality() const
{
  return upscalingQuality;
}

bool DebugWindow::GetUpdateTextureStreaming() const
{
  return updateTextureStreaming;
}

float DebugWindow::GetManualExposure() const
{
  return manualExposure;
}

int DebugWindow::GetDebugTextureIndex() const
{
  return renderTexture ? renderTextureIndex : -1;
}

int DebugWindow::GetDebugHeapTextureIndex() const
{
  return renderHeapTexture ? renderHeapTextureIndex : -1;
}

bool DebugWindow::GetFreezeCulling() const
{
  return freezeCulling;
}

DebugOutput DebugWindow::GetDebugOutput() const
{
  return debugOutput;
}
