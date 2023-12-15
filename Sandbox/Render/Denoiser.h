#pragma once

#include "Types.h"

struct CommandAllocator;
struct CommandList;
struct CommandQueue;
struct ComputeShader;
struct Device;
struct Resource;

struct Denoiser
{
  struct DenoiseResult
  {
    Resource* globalIllumination = nullptr;
    Resource* ambientOcclusion   = nullptr;
    Resource* shadow             = nullptr;
    Resource* reflection         = nullptr;
    Resource* validation         = nullptr;
  };

  using TriangleSetupCallback = eastl::function< int( ComputeShader&, CommandList& ) >;

  virtual ~Denoiser() = default;

  virtual const XMFLOAT4& GetHitDistanceParams() const = 0;

  virtual void TearDown( CommandList* commandList ) = 0;

  virtual void Preprocess( CommandList& commandList, TriangleSetupCallback triangleSetupCallback, float nearZ, float farZ ) = 0;
  virtual DenoiseResult Denoise( CommandAllocator& commandAllocator
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
                               , bool enableValidation ) = 0;
};

eastl::unique_ptr< Denoiser > CreateDenoiser( Device& device, CommandQueue& directQueue, CommandList& commandList, int width, int height );