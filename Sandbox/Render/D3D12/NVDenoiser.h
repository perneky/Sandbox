#pragma once

#include "../Denoiser.h"

struct ComputeShader;
struct NriInterface;
class NrdIntegration;
namespace nri
{
  struct Device;
  struct TextureTransitionBarrierDesc;
}
namespace nrd
{
  struct CommonSettings;
}

class NVDenoiser : public Denoiser
{
public:
  NVDenoiser( Device& device, CommandQueue& commandQueue, CommandList& commandList, int width, int height );
  ~NVDenoiser();

  const XMFLOAT4& GetHitDistanceParams() const override;

  void TearDown( CommandList* commandList ) override;

  void Preprocess( CommandList& commandList, TriangleSetupCallback triangleSetupCallback, float nearZ, float farZ ) override;
  DenoiseResult Denoise( CommandAllocator& commandAllocator
                       , CommandList& commandList
                       , Resource& aoTexture
                       , Resource& shadowTexture
                       , Resource& shadowTransTexture
                       , Resource& reflectionTexture
                       , CXMMATRIX viewTransform
                       , CXMMATRIX projTransform
                       , float jitterX
                       , float jitterY
                       , uint32_t frameIndex
                       , bool enableValidation ) override;

private:
  eastl::unique_ptr< NriInterface >   nriInterface;
  eastl::unique_ptr< NrdIntegration > nrdInterface;
  nri::Device*                        nriDevice;

  eastl::unique_ptr< Resource > internalTextures[ 7 ];
  eastl::vector< nri::TextureTransitionBarrierDesc > nrdInternalTextures;

  eastl::unique_ptr< ComputeShader > preparationShader;

  eastl::unique_ptr< nrd::CommonSettings > commonSettings;
};