#include "NVDenoiser.h"
#include "../RenderManager.h"
#include "../ShaderValues.h"
#include "D3DDevice.h"
#include "D3DCommandAllocator.h"
#include "D3DCommandList.h"
#include "D3DCommandQueue.h"
#include "D3DComputeShader.h"
#include "D3DResource.h"
#include "Common/Files.h"

#include "NRI.h"
#include "Extensions/NRIWrapperD3D12.h"
#include "Extensions/NRIHelper.h"

namespace nri { static const uint32_t WHOLE_DEVICE_GROUP = nri::ALL_NODES; }

#include "NRD.h"
#include "NRDIntegration.hpp"

static constexpr uint32_t AmbientOcclusionId   = 1;
static constexpr uint32_t ShadowId             = 2;
static constexpr uint32_t ReflectionId         = 3;
static constexpr uint32_t GlobalIlluminationId = 4;

static constexpr nrd::HitDistanceParameters hitDistParams = {};

enum InternalTextures
{
  Motion3D,
  NormalRoughness,
  ViewZ,
  FilteredGI,
  FilteredAO,
  FilteredShadow,
  FilteredReflection,
  Validation,
};

struct NriInterface : public nri::CoreInterface, public nri::HelperInterface, public nri::WrapperD3D12Interface
{
};

eastl::unique_ptr< Denoiser > CreateDenoiser( Device& device, CommandQueue& commandQueue, CommandList& commandList, int width, int height )
{
  return eastl::make_unique< NVDenoiser >( device, commandQueue, commandList, width, height );
}

NVDenoiser::NVDenoiser( Device& device, CommandQueue& commandQueue, CommandList& commandList, int width, int height )
  : nriInterface( new NriInterface )
{
  nri::DeviceCreationD3D12Desc deviceDesc = {};
  deviceDesc.d3d12Device         = static_cast< D3DDevice& >( device ).GetD3DDevice();
  deviceDesc.d3d12GraphicsQueue  = static_cast< D3DCommandQueue& >( commandQueue ).GetD3DCommandQueue();
  deviceDesc.enableNRIValidation = false;

  auto nriResult = nri::nriCreateDeviceFromD3D12Device( deviceDesc, nriDevice ); assert( nriResult == nri::Result::SUCCESS );
  nriResult = nriGetInterface( *nriDevice, NRI_INTERFACE( nri::CoreInterface ), (nri::CoreInterface*)nriInterface.get() ); assert( nriResult == nri::Result::SUCCESS );
  nriResult = nriGetInterface( *nriDevice, NRI_INTERFACE( nri::HelperInterface ), (nri::HelperInterface*)nriInterface.get() ); assert( nriResult == nri::Result::SUCCESS );
  nriResult = nriGetInterface( *nriDevice, NRI_INTERFACE( nri::WrapperD3D12Interface ), (nri::WrapperD3D12Interface*)nriInterface.get() ); assert( nriResult == nri::Result::SUCCESS );

  const nrd::DenoiserDesc denoiserDescs[] =
  {
    { AmbientOcclusionId,   nrd::Denoiser::REBLUR_DIFFUSE_OCCLUSION,  uint16_t( width ), uint16_t( height ) },
    { ShadowId,             nrd::Denoiser::SIGMA_SHADOW_TRANSLUCENCY, uint16_t( width ), uint16_t( height ) },
    { ReflectionId,         nrd::Denoiser::REBLUR_SPECULAR,           uint16_t( width ), uint16_t( height ) },
    { GlobalIlluminationId, nrd::Denoiser::REBLUR_DIFFUSE,            uint16_t( width ), uint16_t( height ) },
  };

  nrd::InstanceCreationDesc instanceCreationDesc = {};
  instanceCreationDesc.denoisers    = denoiserDescs;
  instanceCreationDesc.denoisersNum = _countof( denoiserDescs );

  nrdInterface = eastl::make_unique< NrdIntegration >( 2, true );
  auto nrdResult = nrdInterface->Initialize( instanceCreationDesc, *nriDevice, *nriInterface, *nriInterface ); assert( nrdResult );

  internalTextures[ InternalTextures::Motion3D           ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::RGBA16161616F, false, DenoiseMotionSRVSlot,             DenoiseMotionUAVSlot,             1, L"DenoiseMotion" );
  internalTextures[ InternalTextures::ViewZ              ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::R32F,          false, DenoiseViewZSRVSlot,              DenoiseViewZUAVSlot,              1, L"DenoiseViewZ" );
  internalTextures[ InternalTextures::NormalRoughness    ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::RGBA8888UN,    false, DenoiseNormalRoughnessSRVSlot,    DenoiseNormalRoughnessUAVSlot,    1, L"NormalRoughness" );
  internalTextures[ InternalTextures::FilteredGI         ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::RGBA16161616F, false, DenoiseFilteredGISRVSlot,         DenoiseFilteredGIUAVSlot,         1, L"FilteredGI" );
  internalTextures[ InternalTextures::FilteredAO         ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::R16F,          false, DenoiseFilteredAOSRVSlot,         DenoiseFilteredAOUAVSlot,         1, L"FilteredAO" );
  internalTextures[ InternalTextures::FilteredShadow     ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::RGBA8888UN,    false, DenoiseFilteredShadowSRVSlot,     DenoiseFilteredShadowUAVSlot,     1, L"FilteredShadow" );
  internalTextures[ InternalTextures::FilteredReflection ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::RGBA16161616F, false, DenoiseFilteredReflectionSRVSlot, DenoiseFilteredReflectionUAVSlot, 1, L"FilteredReflection" );
  internalTextures[ InternalTextures::Validation         ] = device.Create2DTexture( commandList, width, height, nullptr, 0, PixelFormat::RGBA8888UN,    false, DenoiseValidationSRVSlot,         DenoiseValidationUAVSlot,         1, L"Validation" );

  commandList.ChangeResourceState( { { *internalTextures[ InternalTextures::Motion3D ],        ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::ViewZ ],           ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::NormalRoughness ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } } );

  nri::TextureD3D12Desc textureDesc = {};

  nrdInternalTextures.resize( _countof( internalTextures ), {} );

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::Motion3D ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::Motion3D ].texture );
  nrdInternalTextures[ InternalTextures::Motion3D ].nextAccess = nri::AccessBits::SHADER_RESOURCE;
  nrdInternalTextures[ InternalTextures::Motion3D ].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::ViewZ ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::ViewZ ].texture );
  nrdInternalTextures[ InternalTextures::ViewZ ].nextAccess = nri::AccessBits::SHADER_RESOURCE;
  nrdInternalTextures[ InternalTextures::ViewZ ].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::NormalRoughness ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::NormalRoughness ].texture );
  nrdInternalTextures[ InternalTextures::NormalRoughness ].nextAccess = nri::AccessBits::SHADER_RESOURCE;
  nrdInternalTextures[ InternalTextures::NormalRoughness ].nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::FilteredGI ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::FilteredGI ].texture );
  nrdInternalTextures[ InternalTextures::FilteredGI ].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
  nrdInternalTextures[ InternalTextures::FilteredGI ].nextLayout = nri::TextureLayout::GENERAL;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::FilteredAO ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::FilteredAO ].texture );
  nrdInternalTextures[ InternalTextures::FilteredAO ].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
  nrdInternalTextures[ InternalTextures::FilteredAO ].nextLayout = nri::TextureLayout::GENERAL;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::FilteredShadow ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::FilteredShadow ].texture );
  nrdInternalTextures[ InternalTextures::FilteredShadow ].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
  nrdInternalTextures[ InternalTextures::FilteredShadow ].nextLayout = nri::TextureLayout::GENERAL;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::FilteredReflection ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::FilteredReflection ].texture );
  nrdInternalTextures[ InternalTextures::FilteredReflection ].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
  nrdInternalTextures[ InternalTextures::FilteredReflection ].nextLayout = nri::TextureLayout::GENERAL;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( *internalTextures[ InternalTextures::Validation ] ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)nrdInternalTextures[ InternalTextures::Validation ].texture );
  nrdInternalTextures[ InternalTextures::Validation ].nextAccess = nri::AccessBits::SHADER_RESOURCE_STORAGE;
  nrdInternalTextures[ InternalTextures::Validation ].nextLayout = nri::TextureLayout::GENERAL;

  auto preparationFile = ReadFileToMemory( L"Content/Shaders/PrepareDenoiser.cso" );
  preparationShader = device.CreateComputeShader( preparationFile.data(), int( preparationFile.size() ), L"Denoiser preparation" );

  commonSettings = eastl::make_unique< nrd::CommonSettings >();
}

NVDenoiser::~NVDenoiser()
{
}

const XMFLOAT4& NVDenoiser::GetHitDistanceParams() const
{
  return *(XMFLOAT4*)&hitDistParams;
}

void NVDenoiser::TearDown( CommandList* commandList )
{
  for ( auto& texture : nrdInternalTextures )
    nriInterface->DestroyTexture( (nri::Texture&)*texture.texture );

  if ( commandList )
    for ( auto& texture : internalTextures )
      commandList->HoldResource( eastl::move( texture ) );

  nrdInterface->Destroy();
  nrdInterface.reset();

  nri::nriDestroyDevice( *nriDevice );
  nriInterface.reset();
}

void NVDenoiser::Preprocess( CommandList& commandList, TriangleSetupCallback triangleSetupCallback, float nearZ, float farZ )
{
  GPUSection gpuSection( commandList, L"Denoiser preprocess" );

  struct
  {
    uint32_t width;
    uint32_t height;
    float    nearZ;
    float    farZ;
  } constants;

  constants.width  = internalTextures[ 0 ]->GetTextureWidth();
  constants.height = internalTextures[ 0 ]->GetTextureHeight();
  constants.nearZ  = nearZ;
  constants.farZ   = farZ;

  commandList.ChangeResourceState( { { *internalTextures[ InternalTextures::NormalRoughness ], ResourceStateBits::UnorderedAccess }
                                   , { *internalTextures[ InternalTextures::ViewZ ],           ResourceStateBits::UnorderedAccess } } );

  auto base = triangleSetupCallback( *preparationShader, commandList );

  commandList.SetComputeUnorderedAccessView( base + 0, *internalTextures[ InternalTextures::NormalRoughness ] );
  commandList.SetComputeUnorderedAccessView( base + 1, *internalTextures[ InternalTextures::ViewZ ] );
  commandList.SetComputeUnorderedAccessView( base + 2, *internalTextures[ InternalTextures::Motion3D ] );
  commandList.SetComputeConstantValues( base + 3, constants, 0 );

  commandList.Dispatch( ( internalTextures[ 0 ]->GetTextureWidth () + PrepareDenoiserKernelWidth  - 1 ) / PrepareDenoiserKernelWidth
                      , ( internalTextures[ 0 ]->GetTextureHeight() + PrepareDenoiserKernelHeight - 1 ) / PrepareDenoiserKernelHeight
                      , 1 );

  commandList.AddUAVBarrier( { *internalTextures[ 0 ], *internalTextures[ 1 ] } );

  commandList.ChangeResourceState( { { *internalTextures[ InternalTextures::NormalRoughness ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::ViewZ ],           ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } } );
}

Denoiser::DenoiseResult NVDenoiser::Denoise( CommandAllocator& commandAllocator
                                           , CommandList& commandList
                                           , Resource& giTexture
                                           , Resource* aoTexture
                                           , Resource& shadowTexture
                                           , Resource& shadowTransTexture
                                           , Resource& reflectionTexture
                                           , CXMMATRIX viewTransform
                                           , CXMMATRIX projTransform
                                           , float jitterX
                                           , float jitterY
                                           , uint32_t frameIndex
                                           , bool enableValidation )
{
  GPUSection gpuSection( commandList, L"Denoiser denoise" );

  commandList.ChangeResourceState( { { giTexture, ResourceStateBits::NonPixelShaderInput }
                                   , { shadowTexture, ResourceStateBits::NonPixelShaderInput }
                                   , { shadowTransTexture, ResourceStateBits::NonPixelShaderInput }
                                   , { reflectionTexture, ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::FilteredAO ], ResourceStateBits::UnorderedAccess }
                                   , { *internalTextures[ InternalTextures::FilteredGI ], ResourceStateBits::UnorderedAccess }
                                   , { *internalTextures[ InternalTextures::FilteredShadow ], ResourceStateBits::UnorderedAccess }
                                   , { *internalTextures[ InternalTextures::FilteredReflection ], ResourceStateBits::UnorderedAccess }
                                   , { *internalTextures[ InternalTextures::Validation ], ResourceStateBits::UnorderedAccess } } );

  if ( aoTexture )
    commandList.ChangeResourceState( *aoTexture, ResourceStateBits::NonPixelShaderInput );

  // Prepare
  nri::CommandBufferD3D12Desc commandBufferDesc = {};
  commandBufferDesc.d3d12CommandList      = static_cast< D3DCommandList& >( commandList ).GetD3DGraphicsCommandList();
  commandBufferDesc.d3d12CommandAllocator = static_cast< D3DCommandAllocator& >( commandAllocator ).GetD3DCommandAllocator();

  nri::CommandBuffer* nriCommandBuffer = nullptr;
  nriInterface->CreateCommandBufferD3D12( *nriDevice, commandBufferDesc, nriCommandBuffer );

  nri::TextureTransitionBarrierDesc giDesc;
  nri::TextureTransitionBarrierDesc aoDesc;
  nri::TextureTransitionBarrierDesc shadowDesc;
  nri::TextureTransitionBarrierDesc shadowTransDesc;
  nri::TextureTransitionBarrierDesc reflectionDesc;

  nri::TextureD3D12Desc textureDesc = {};

  textureDesc.d3d12Resource = static_cast< D3DResource& >( giTexture ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)giDesc.texture );
  giDesc.nextAccess = nri::AccessBits::SHADER_RESOURCE;
  giDesc.nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  if ( aoTexture )
  {
    textureDesc.d3d12Resource = static_cast< D3DResource& >( *aoTexture ).GetD3DResource();
    nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)aoDesc.texture );
    aoDesc.nextAccess = nri::AccessBits::SHADER_RESOURCE;
    aoDesc.nextLayout = nri::TextureLayout::SHADER_RESOURCE;
  }

  textureDesc.d3d12Resource = static_cast< D3DResource& >( shadowTexture ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)shadowDesc.texture );
  shadowDesc.nextAccess = nri::AccessBits::SHADER_RESOURCE;
  shadowDesc.nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( shadowTransTexture ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)shadowTransDesc.texture );
  shadowTransDesc.nextAccess = nri::AccessBits::SHADER_RESOURCE;
  shadowTransDesc.nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  textureDesc.d3d12Resource = static_cast< D3DResource& >( reflectionTexture ).GetD3DResource();
  nriInterface->CreateTextureD3D12( *nriDevice, textureDesc, (nri::Texture*&)reflectionDesc.texture );
  reflectionDesc.nextAccess = nri::AccessBits::SHADER_RESOURCE;
  reflectionDesc.nextLayout = nri::TextureLayout::SHADER_RESOURCE;

  nrdInterface->NewFrame();

  // Denoise
  memcpy_s( commonSettings->viewToClipMatrixPrev,  sizeof( commonSettings->viewToClipMatrixPrev ),  commonSettings->viewToClipMatrix,  sizeof( commonSettings->viewToClipMatrix  ) );
  memcpy_s( commonSettings->worldToViewMatrixPrev, sizeof( commonSettings->worldToViewMatrixPrev ), commonSettings->worldToViewMatrix, sizeof( commonSettings->worldToViewMatrix ) );
  memcpy_s( commonSettings->cameraJitterPrev,      sizeof( commonSettings->cameraJitterPrev ),      commonSettings->cameraJitter,      sizeof( commonSettings->cameraJitter      ) );
  XMStoreFloat4x4( (XMFLOAT4X4*)commonSettings->worldToViewMatrix, viewTransform );
  XMStoreFloat4x4( (XMFLOAT4X4*)commonSettings->viewToClipMatrix,  projTransform );
  commonSettings->denoisingRange             = 2000;
  commonSettings->frameIndex                 = frameIndex;
  commonSettings->cameraJitter[ 0 ]          = jitterX;
  commonSettings->cameraJitter[ 1 ]          = jitterY;
  commonSettings->motionVectorScale[ 0 ]     =
  commonSettings->motionVectorScale[ 1 ]     =
  commonSettings->motionVectorScale[ 2 ]     = 1;
  commonSettings->isMotionVectorInWorldSpace = true;
  commonSettings->enableValidation           = enableValidation;
  nrdInterface->SetCommonSettings( *commonSettings );

  nrd::ReblurSettings reblurSettings;
  reblurSettings.hitDistanceParameters  = hitDistParams;
  reblurSettings.maxAccumulatedFrameNum = MAX_ACCUM_DENOISE_FRAMES;
  reblurSettings.checkerboardMode       = nrd::CheckerboardMode::WHITE;
  nrdInterface->SetDenoiserSettings( AmbientOcclusionId,   &reblurSettings );
  nrdInterface->SetDenoiserSettings( ReflectionId,         &reblurSettings );
  nrdInterface->SetDenoiserSettings( GlobalIlluminationId, &reblurSettings );

  nrd::SigmaSettings sigmaSettings;
  nrdInterface->SetDenoiserSettings( ShadowId, &sigmaSettings );

  NrdUserPool userPool = {};
  {
    if ( aoTexture )
      NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_DIFF_HITDIST, { &aoDesc, nri::Format::R8_UNORM } );

    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_SHADOWDATA,             { &shadowDesc, nri::Format::RG16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_SHADOW_TRANSLUCENCY,    { &shadowTransDesc, nri::Format::RGBA8_UNORM } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST,  { &reflectionDesc, nri::Format::RGBA16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,  { &giDesc, nri::Format::RGBA16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_MV,                     { &nrdInternalTextures[ InternalTextures::Motion3D ], nri::Format::RGBA16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_NORMAL_ROUGHNESS,       { &nrdInternalTextures[ InternalTextures::NormalRoughness ], nri::Format::RGBA8_UNORM } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::IN_VIEWZ,                  { &nrdInternalTextures[ InternalTextures::ViewZ ], nri::Format::R32_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::OUT_DIFF_HITDIST,          { &nrdInternalTextures[ InternalTextures::FilteredAO ], nri::Format::R16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY,   { &nrdInternalTextures[ InternalTextures::FilteredShadow ], nri::Format::RGBA8_UNORM } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST, { &nrdInternalTextures[ InternalTextures::FilteredReflection ], nri::Format::RGBA16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST, { &nrdInternalTextures[ InternalTextures::FilteredGI ], nri::Format::RGBA16_SFLOAT } );
    NrdIntegration_SetResource( userPool, nrd::ResourceType::OUT_VALIDATION,            { &nrdInternalTextures[ InternalTextures::Validation ], nri::Format::RGBA8_UNORM } );
  };

  const nrd::Identifier denoisers[] = { GlobalIlluminationId, ReflectionId, ShadowId, AmbientOcclusionId };
  
  nrdInterface->Denoise( denoisers, aoTexture ? _countof( denoisers ) : _countof( denoisers ) - 1, *nriCommandBuffer, userPool );

  // Cleanup
  if ( aoTexture )
    nriInterface->DestroyTexture( (nri::Texture&)*aoDesc.texture );
  nriInterface->DestroyTexture( (nri::Texture&)*giDesc.texture );
  nriInterface->DestroyTexture( (nri::Texture&)*shadowDesc.texture );
  nriInterface->DestroyTexture( (nri::Texture&)*shadowTransDesc.texture );
  nriInterface->DestroyTexture( (nri::Texture&)*reflectionDesc.texture );
  nriInterface->DestroyCommandBuffer( *nriCommandBuffer );

  commandList.ChangeResourceState( { { *internalTextures[ InternalTextures::FilteredAO ],         ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::FilteredGI ],         ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::FilteredShadow ],     ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::FilteredReflection ], ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput }
                                   , { *internalTextures[ InternalTextures::Validation ],         ResourceStateBits::PixelShaderInput | ResourceStateBits::NonPixelShaderInput } } );

  return DenoiseResult { .globalIllumination = internalTextures[ InternalTextures::FilteredGI ].get()
                       , .ambientOcclusion   = aoTexture ? internalTextures[ InternalTextures::FilteredAO ].get() : nullptr
                       , .shadow             = internalTextures[ InternalTextures::FilteredShadow ].get()
                       , .reflection         = internalTextures[ InternalTextures::FilteredReflection ].get()
                       , .validation         = internalTextures[ InternalTextures::Validation ].get() };
}
