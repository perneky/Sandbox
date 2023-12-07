#include "D3DSwapchain.h"
#include "D3DFactory.h"
#include "D3DCommandQueue.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "D3DResourceDescriptor.h"
#include "Platform/Windows/WinAPIWindow.h"

static constexpr int BackBufferCount = 2;

D3DSwapchain::D3DSwapchain( D3DFactory& factory, D3DCommandQueue& queue, D3DDevice& device, WinAPIWindow& window )
{
  auto dxgiFactory = factory.GetDXGIFactory();

  isTearingSupported = factory.IsVRRSupported();
  isTearingEnabled   = isTearingSupported;

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width       = window.GetClientWidth();
  swapChainDesc.Height      = window.GetClientHeight();
  swapChainDesc.Format      = DXGI_FORMAT_R10G10B10A2_UNORM;
  swapChainDesc.Stereo      = FALSE;
  swapChainDesc.SampleDesc  = { 1, 0 };
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = BackBufferCount;
  swapChainDesc.Scaling     = DXGI_SCALING_NONE;
  swapChainDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;
  swapChainDesc.Flags       = isTearingEnabled ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  CComPtr< IDXGISwapChain1 > swapChain1;
  dxgiFactory->CreateSwapChainForHwnd( queue.GetD3DCommandQueue()
                                     , window.GetWindowHandle()
                                     , &swapChainDesc
                                     , nullptr
                                     , nullptr
                                     , &swapChain1 );

  dxgiFactory->MakeWindowAssociation( window.GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER );

  swapChain1->QueryInterface( &dxgiSwapchain );

  D3D12_DESCRIPTOR_HEAP_DESC bbHeapDesc = {};
  bbHeapDesc.NumDescriptors = BackBufferCount;
  bbHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  bbHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  device.GetD3DDevice()->CreateDescriptorHeap( &bbHeapDesc, IID_PPV_ARGS( &d3dBackBufferDescriptors ) );

  currentFrameIndex = dxgiSwapchain->GetCurrentBackBufferIndex();
}

void D3DSwapchain::BuildBackBufferTextures( Device& device )
{
  auto d3dDevice = static_cast< D3DDevice* >( &device )->GetD3DDevice();

  auto rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
  auto rtvHandle         = d3dBackBufferDescriptors->GetCPUDescriptorHandleForHeapStart();

  frameData.resize( BackBufferCount );
  for ( auto index = 0U; index < BackBufferCount; ++index )
  {
    CComPtr< ID3D12Resource2 > backBuffer;
    dxgiSwapchain->GetBuffer( index, IID_PPV_ARGS( &backBuffer ) );

    auto bufferName = eastl::wstring( L"backBuffer_" ) + eastl::to_wstring( index );
    backBuffer->SetName( bufferName.data() );

    d3dDevice->CreateRenderTargetView( backBuffer, nullptr, rtvHandle );

    frameData[ index ].texture.reset( new D3DResource( AllocatedResource( backBuffer ), ResourceStateBits::Present ) );
    frameData[ index ].texture->AttachResourceDescriptor( ResourceDescriptorType::RenderTargetView, eastl::unique_ptr< ResourceDescriptor >( new D3DResourceDescriptor( rtvHandle ) ) );

    rtvHandle.ptr += rtvDescriptorSize;
  }
}

Resource& D3DSwapchain::GetCurrentBackBufferTexture()
{
  return *frameData[ currentFrameIndex ].texture;
}

int D3DSwapchain::GetCurrentBackBufferTextureIndex()
{
  return dxgiSwapchain->GetCurrentBackBufferIndex();
}

uint64_t D3DSwapchain::Present( uint64_t fenceValue, bool useVSync )
{
  auto result = dxgiSwapchain->Present( useVSync ? 1 : 0, isTearingEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0 );
  #if ENBALE_AFTERMATH
    if ( result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET )
      std::this_thread::sleep_for( std::chrono::milliseconds( 100000 ) );
  #endif

  frameData[ currentFrameIndex ].fenceValue = fenceValue;

  currentFrameIndex = dxgiSwapchain->GetCurrentBackBufferIndex();
  return frameData[ currentFrameIndex ].fenceValue;
}

void D3DSwapchain::ToggleFullscreen( CommandList& commandList, Device& device, Window& window )
{
  frameData.clear();

  DXGI_SWAP_CHAIN_DESC1 desc;
  dxgiSwapchain->GetDesc1( &desc );

  DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc = {};
  dxgiSwapchain->GetFullscreenDesc( &fullscreenDesc );

  dxgiSwapchain->SetFullscreenState( fullscreenDesc.Windowed, nullptr );

  if ( fullscreenDesc.Windowed )
  {
    // Going to fullscreen
    POINT zeroPoint = { 0, 0 };
    auto monitor = MonitorFromPoint( zeroPoint, MONITOR_DEFAULTTOPRIMARY );
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof( monitorInfo );
    GetMonitorInfoW( monitor, &monitorInfo );

    int width  = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
    int height = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

    dxgiSwapchain->ResizeBuffers( desc.BufferCount, width, height, desc.Format, 0 );
    isTearingEnabled = false;
  }
  else
  {
    // Going to windowed
    dxgiSwapchain->ResizeBuffers( desc.BufferCount, window.GetClientWidth(), window.GetClientHeight(), desc.Format, isTearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0 );
    isTearingEnabled = isTearingSupported;
  }

  currentFrameIndex = dxgiSwapchain->GetCurrentBackBufferIndex();

  BuildBackBufferTextures( device );
}

void D3DSwapchain::Resize( CommandList& commandList, Device& device, Window& window )
{
  frameData.clear();

  DXGI_SWAP_CHAIN_DESC1 desc;
  dxgiSwapchain->GetDesc1( &desc );
  dxgiSwapchain->ResizeBuffers( desc.BufferCount, window.GetClientWidth(), window.GetClientHeight(), desc.Format, desc.Flags );

  currentFrameIndex = dxgiSwapchain->GetCurrentBackBufferIndex();

  BuildBackBufferTextures( device );
}

D3DSwapchain::~D3DSwapchain()
{
  if ( isFullscreen )
    dxgiSwapchain->SetFullscreenState( FALSE, nullptr );
}

IDXGISwapChain4* D3DSwapchain::GetDXGISwapchain()
{
  return dxgiSwapchain;
}
