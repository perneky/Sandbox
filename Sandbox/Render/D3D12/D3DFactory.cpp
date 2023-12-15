#include "D3DFactory.h"
#include "D3DAdapter.h"
#include "D3DDevice.h"
#include "D3DSwapchain.h"
#include "D3DCommandQueue.h"
#include "Platform/Windows/WinAPIWindow.h"
#include "WinPixEventRuntime/pix3.h"

extern "C" { __declspec(dllexport) extern const UINT  D3D12SDKVersion = 611; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath    = ".\\D3D12\\"; }

#if ENBALE_AFTERMATH
  #include "GFSDK_Aftermath.h"
  #include "GFSDK_Aftermath_GpuCrashDump.h"
  #include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"
  #pragma comment( lib, "GFSDK_Aftermath_Lib.x64.lib" )

  void WriteAftermathDumpToFile( const void* pGpuCrashDump, const uint32_t gpuCrashDumpSize, void* pUserData )
  {
    static uint32_t dumpIndex = 0;
    eastl::string     dumpName  = "GpuCrashDump_" + eastl::to_string( dumpIndex++ ) + ".nv-gpudmp";
    FILE*           dumpFile  = nullptr;
    fopen_s( &dumpFile, dumpName.data(), "wb" );
    fwrite( pGpuCrashDump, gpuCrashDumpSize, 1, dumpFile );
    fclose( dumpFile );
  }

  void WriteAftermathShaderDebugInfoToFile( const void* pShaderDebugInfo, const uint32_t shaderDebugInfoSize, void* pUserData )
  {
    GFSDK_Aftermath_ShaderDebugInfoIdentifier identifier = {};
    GFSDK_Aftermath_GetShaderDebugInfoIdentifier( GFSDK_Aftermath_Version_API, pShaderDebugInfo, shaderDebugInfoSize, &identifier );

    eastl::string     dumpName  = "GpuCrashDump_" + eastl::to_string( identifier.id[0] ) + "_" + eastl::to_string( identifier.id[ 1 ] ) + ".nv-gpudmp-shader";
    FILE*           dumpFile  = nullptr;
    fopen_s( &dumpFile, dumpName.data(), "wb" );
    fwrite( pShaderDebugInfo, shaderDebugInfoSize, 1, dumpFile );
    fclose( dumpFile );
  }
#endif

void EnableAftermathIfNeeded( ID3D12Device* device )
{
  #if ENBALE_AFTERMATH
    const uint32_t aftermathFlags = GFSDK_Aftermath_FeatureFlags_EnableMarkers
                                  | GFSDK_Aftermath_FeatureFlags_CallStackCapturing
                                  | GFSDK_Aftermath_FeatureFlags_EnableResourceTracking
                                  | GFSDK_Aftermath_FeatureFlags_GenerateShaderDebugInfo
                                  | GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting;

    GFSDK_Aftermath_Result amResult = GFSDK_Aftermath_DX12_Initialize( GFSDK_Aftermath_Version_API, aftermathFlags, device );
    assert( amResult == GFSDK_Aftermath_Result_Success );
#endif
}

eastl::unique_ptr< Factory > Factory::Create()
{
  return eastl::unique_ptr< Factory >( new D3DFactory );
}

D3DFactory::D3DFactory()
{
  #if ENBALE_AFTERMATH
    GFSDK_Aftermath_Result amResult = GFSDK_Aftermath_EnableGpuCrashDumps( GFSDK_Aftermath_Version_API
                                                                         , GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX
                                                                         , GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default
                                                                         , WriteAftermathDumpToFile
                                                                         , WriteAftermathShaderDebugInfoToFile
                                                                         , nullptr
                                                                         , nullptr
                                                                         , nullptr );
    assert( amResult == GFSDK_Aftermath_Result_Success );
  #endif

  UINT createFactoryFlags = 0;

  #if DEBUG_GFX_API
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
    
    CComPtr< ID3D12Debug5 > debugInterface;
    D3D12GetDebugInterface( IID_PPV_ARGS( &debugInterface ) );
    debugInterface->EnableDebugLayer();
    
    #if 0
      debugInterface->SetEnableAutoName( TRUE );
      debugInterface->SetEnableGPUBasedValidation( TRUE );
      debugInterface->SetEnableSynchronizedCommandQueueValidation( TRUE );
    #endif
  #endif // DEBUG_GFX_API

  BOOL allowTearing = FALSE;

  if SUCCEEDED( CreateDXGIFactory2( createFactoryFlags, IID_PPV_ARGS( &dxgiFactory ) ) )
    if FAILED( dxgiFactory->CheckFeatureSupport( DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof( allowTearing ) ) )
      allowTearing = FALSE;

  variableRefreshRateSupported = allowTearing != FALSE;
}

D3DFactory::~D3DFactory()
{
}
IDXGIFactoryX* D3DFactory::GetDXGIFactory()
{
  return dxgiFactory;
}

bool D3DFactory::IsVRRSupported() const
{
  return variableRefreshRateSupported;
}

eastl::unique_ptr< Adapter > D3DFactory::CreateDefaultAdapter()
{
  return eastl::unique_ptr< Adapter >( new D3DAdapter( *this ) );
}

eastl::unique_ptr< Swapchain > D3DFactory::CreateSwapchain( Device& device, CommandQueue& directQueue, Window& window )
{
  return eastl::unique_ptr< Swapchain >( new D3DSwapchain( *this
                                                         , *static_cast< D3DCommandQueue* >( &directQueue )
                                                         , *static_cast< D3DDevice* >( &device )
                                                         , *static_cast< WinAPIWindow* >( &window ) ) );
}
