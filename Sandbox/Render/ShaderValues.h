#pragma once

#ifndef SHADER_VARS_H
#define SHADER_VARS_H

#ifdef __cplusplus
  #include "TextureStreamers/TFFFormat.h"
#endif // __cplusplus

#define PI   3.141592654
#define PIPI ( 3.141592654 * 2 )
#define INVPI ( 1.0f / 3.141592654 )

#define INF 1e5

#define Engine2DResourceCount         70
#define EngineCubeResourceCount       10
#define EngineVolResourceCount        10
#define EngineBufferResourceCount     40
#define Scene2DResourceCount          300
#define SceneBufferResourceCount      5000
#define Engine2DReferenceTextureCount 12
#define Engine2DTileTexturesCount     100

#define AllResourceCount ( Engine2DResourceCount + EngineCubeResourceCount + EngineVolResourceCount + EngineBufferResourceCount + Scene2DResourceCount * 3 + SceneBufferResourceCount + Engine2DReferenceTextureCount + Engine2DTileTexturesCount )

#define Engine2DResourceBaseSlot         ( 0 )
#define EngineCubeResourceBaseSlot       ( Engine2DResourceBaseSlot         + Engine2DResourceCount )
#define EngineVolResourceBaseSlot        ( EngineCubeResourceBaseSlot       + EngineCubeResourceCount )
#define EngineBufferResourceBaseSlot     ( EngineVolResourceBaseSlot        + EngineVolResourceCount )
#define Scene2DResourceBaseSlot          ( EngineBufferResourceBaseSlot     + EngineBufferResourceCount )
#define Scene2DFeedbackBaseSlot          ( Scene2DResourceBaseSlot          + Scene2DResourceCount )
#define Scene2DMipTailBaseSlot           ( Scene2DFeedbackBaseSlot          + Scene2DResourceCount )
#define SceneBufferResourceBaseSlot      ( Scene2DMipTailBaseSlot           + Scene2DResourceCount )
#define Engine2DReferenceTextureBaseSlot ( SceneBufferResourceBaseSlot      + SceneBufferResourceCount )
#define Engine2DTileTexturesBaseSlot     ( Engine2DReferenceTextureBaseSlot + Engine2DReferenceTextureCount )

#define Engine2DResourceCountStr         "70"
#define EngineCubeResourceCountStr       "10"
#define EngineVolResourceCountStr        "10"
#define EngineBufferResourceCountStr     "40"
#define Scene2DResourceCountStr          "300"
#define SceneBufferResourceCountStr      "5000"
#define Engine2DReferenceTextureCountStr "12"
#define Engine2DTileTexturesCountStr     "100"

#ifdef __cplusplus
  static_assert( Engine2DResourceCount == atou_cex( Engine2DResourceCountStr ), "Engine2DResourceCountStr is wrong" );
  static_assert( EngineCubeResourceCount == atou_cex( EngineCubeResourceCountStr ), "EngineCubeResourceCountStr is wrong" );
  static_assert( EngineVolResourceCount == atou_cex( EngineVolResourceCountStr ), "EngineVolResourceCountStr is wrong" );
  static_assert( EngineBufferResourceCount == atou_cex( EngineBufferResourceCountStr ), "EngineBufferResourceCountStr is wrong" );
  static_assert( Scene2DResourceCount == atou_cex( Scene2DResourceCountStr ), "Scene2DResourceCountStr is wrong" );
  static_assert( SceneBufferResourceCount == atou_cex( SceneBufferResourceCountStr ), "SceneBufferResourceCountStr is wrong" );
  static_assert( Engine2DReferenceTextureCount == atou_cex( Engine2DReferenceTextureCountStr ), "Engine2DReferenceTextureCountStr is wrong" );
  static_assert( Engine2DTileTexturesCount == atou_cex( Engine2DTileTexturesCountStr ), "Engine2DTileTexturesCountStr is wrong" );
#endif // __cplusplus

enum Texture2DSlots
{
  ColorTextureSlot
  #ifdef __cplusplus
    = Engine2DResourceBaseSlot
  #endif // __cplusplus
  ,
  DepthTextureSlot,
  DebugTextureSlot,
  AOTextureSRVSlot,
  AOTextureUAVSlot,
  ReflectionTextureSRVSlot,
  ReflectionTextureUAVSlot,
  GITextureSRVSlot,
  GITextureUAVSlot,
  MotionVectorsSlot,
  TextureMipSlot,
  GeometryIdsSlot,
  ExposureOnlySlot,
  ExposureOnlyUAVSlot,
  SpecBRDFLUTSlot,
  SpecBRDFLUTUAVSlot,
  ReflectionProc0TextureSlot,
  ReflectionProc1TextureSlot,
  ReflectionProc0TextureUAVSlot,
  ReflectionProc1TextureUAVSlot,
  UpscaledTextureSlot,
  UpscaledTextureUAVSlot,
  ShadowTextureSlot,
  ShadowTextureUAVSlot,
  ShadowTransTextureSlot,
  ShadowTransTextureUAVSlot,
  LumaTextureSlot,
  LumaTextureUAVSlot,

  DenoiseMotionSRVSlot,
  DenoiseMotionUAVSlot,
  DenoiseViewZSRVSlot,
  DenoiseViewZUAVSlot,
  DenoiseNormalRoughnessSRVSlot,
  DenoiseNormalRoughnessUAVSlot,
  DenoiseFilteredGISRVSlot,
  DenoiseFilteredGIUAVSlot,
  DenoiseFilteredAOSRVSlot,
  DenoiseFilteredAOUAVSlot,
  DenoiseFilteredShadowSRVSlot,
  DenoiseFilteredShadowUAVSlot,
  DenoiseFilteredReflectionSRVSlot,
  DenoiseFilteredReflectionUAVSlot,
  DenoiseValidationSRVSlot,
  DenoiseValidationUAVSlot,

  UploadTargetDebugSlot,
  ScramblingRankingSlot,

  BloomA0TextureSlot,
  BloomA1TextureSlot,
  BloomA2TextureSlot,
  BloomA3TextureSlot,
  BloomA4TextureSlot,
  BloomB0TextureSlot,
  BloomB1TextureSlot,
  BloomB2TextureSlot,
  BloomB3TextureSlot,
  BloomB4TextureSlot,

  BloomA0TextureUAVSlot,
  BloomA1TextureUAVSlot,
  BloomA2TextureUAVSlot,
  BloomA3TextureUAVSlot,
  BloomA4TextureUAVSlot,
  BloomB0TextureUAVSlot,
  BloomB1TextureUAVSlot,
  BloomB2TextureUAVSlot,
  BloomB3TextureUAVSlot,
  BloomB4TextureUAVSlot,

  Texture2DSlotCount
};

#define Texture2DSlotCountStr "66"

#ifdef __cplusplus
  static_assert( Texture2DSlotCount < Engine2DResourceBaseSlot + Engine2DResourceCount, "Too many engine 2D textures!" );
  static_assert( atou_cex( Texture2DSlotCountStr ) == ( Texture2DSlotCount - Engine2DResourceBaseSlot ), "Engine 2D texture count mismatch!" );
#endif // __cplusplus

enum TextureCubeSlots
{
  SkyTextureSlot
#ifdef __cplusplus
    = EngineCubeResourceBaseSlot
  #endif // __cplusplus
  ,

  TextureCubeSlotCount
};

#define TextureCubeSlotCountStr "1"

#ifdef __cplusplus
  static_assert( TextureCubeSlotCount < EngineCubeResourceBaseSlot + EngineCubeResourceCount, "Too many engine cube textures!" );
  static_assert( atou_cex( TextureCubeSlotCountStr ) == ( TextureCubeSlotCount - EngineCubeResourceBaseSlot ), "Engine cube texture count mismatch!" );
#endif // __cplusplus

enum TextureVolumeSlots
{
  RandomTextureSlot
  #ifdef __cplusplus
    = EngineVolResourceBaseSlot
  #endif // __cplusplus
  ,

  TextureVolumeSlotCount
};

#ifdef __cplusplus
  static_assert( TextureVolumeSlotCount < EngineVolResourceBaseSlot + EngineVolResourceCount, "Too many engine volume textures!" );
#endif // __cplusplus

enum BufferSlots
{
  BaseBufferSlot
#ifdef __cplusplus
  = EngineBufferResourceBaseSlot
#endif // __cplusplus
  ,

  NodeBufferSlot,
  RootNodeChildrenInidcesBufferSlot,
  MeshBufferSlot,
  MaterialBufferSlot,
  CameraBufferSlot,
  LightBufferSlot,
  ModelMetaBufferSlot,
  FrameParamsBufferUAVSlot,
  FrameParamsBufferCBVSlot,
  ProcessedLightBufferUAVSlot,
  ProcessedLightBufferSRVSlot,
  TextureWriteInfoUAVSlot,
  TextureWriteInfoSRVSlot,
  SkyBufferUAVSlot,
  SkyBufferCBVSlot,
  IndirectDrawCountBufferSlot,
  IndirectOpaqueDrawBufferSRVSlot,
  IndirectOpaqueTwoSidedDrawBufferSRVSlot,
  IndirectOpaqueAlphaTestedDrawBufferSRVSlot,
  IndirectOpaqueTwoSidedAlphaTestedDrawBufferSRVSlot,
  IndirectTranslucentDrawBufferSRVSlot,
  IndirectTranslucentTwoSidedDrawBufferSRVSlot,
  IndirectOpaqueDrawBufferUAVSlot,
  IndirectOpaqueTwoSidedDrawBufferUAVSlot,
  IndirectOpaqueAlphaTestedDrawBufferUAVSlot,
  IndirectOpaqueTwoSidedAlphaTestedDrawBufferUAVSlot,
  IndirectTranslucentDrawBufferUAVSlot,
  IndirectTranslucentTwoSidedDrawBufferUAVSlot,
  ExposureBufferCBVSlot,
  ExposureBufferUAVSlot,
  HistogramBufferSlot,
  HistogramBufferUAVSlot,
  RTSceneSlot,
  SobolSlot,

  BufferSlotCount
};

#ifdef __cplusplus
  static_assert( BufferSlotCount < EngineBufferResourceBaseSlot + EngineBufferResourceCount, "Too many engine buffers!" );
#endif // __cplusplus

#define ENABLE_TEXTURE_STREAMING 1

#define USE_REVERSE_PROJECTION 1

#define MAX_ACCUM_DENOISE_FRAMES 30

#define GI_MAX_ITERATIONS 3

#define USE_AO_WITH_GI 0

#define RandomTextureSize  128
#define RandomTextureScale 0.1

#define CullingKernelWidth 64

#define DownsamplingKernelWidth  8
#define DownsamplingKernelHeight 8

#define LightCombinerKernelWidth  8
#define LightCombinerKernelHeight 8

#define ExtractBloomKernelWidth  8
#define ExtractBloomKernelHeight 8

#define DownsampleBloomKernelWidth  8
#define DownsampleBloomKernelHeight 8

#define UpsampleBlurBloomKernelWidth  8
#define UpsampleBlurBloomKernelHeight 8

#define BlurBloomKernelWidth  8
#define BlurBloomKernelHeight 8

#define ToneMappingKernelWidth  8
#define ToneMappingKernelHeight 8

#define ReflectionProcessKernelWidth  8
#define ReflectionProcessKernelHeight 8
#define ReflectionProcessTaps         16

#define GenerateHistogramKernelWidth  16
#define GenerateHistogramKernelHeight 16

#define AdaptExposureKernelWidth  256
#define AdaptExposureKernelHeight 1

#define ProbeTraceKernelWidth  1
#define ProbeTraceKernelHeight 1
#define ProbeTraceKernelDepth  1

#define GIProbeKernelWidth  GIProbeWidth
#define GIProbeKernelHeight GIProbeHeight

#define PrepareDenoiserKernelWidth  8
#define PrepareDenoiserKernelHeight 8

#define SpecBRDFLUTSize 256

// Adding 8 for the borders, 1 BC block on each side
#define TileSize            256
#define TileSizeF           256.0f
#define TileBorder          4
#define TileBorderF         4.0f
#define TileSizeWithBorder  ( TileSize  + TileBorder  * 2 )
#define TileSizeWithBorderF ( TileSizeF + TileBorderF * 2 )
#define TileCount           32
#define TileCountF          32.0F
#define TileTextureSize     ( TileSizeWithBorder  * TileCount  )
#define TileTextureSizeF    ( TileSizeWithBorderF * TileCountF )

#ifdef __cplusplus
  static_assert( TileSize == TFFHeader::tileSize, "Tile size need to match with TFF!" );
#endif // __cplusplus

#if 1
  #define BigRangeFlags ", flags = DESCRIPTORS_VOLATILE"
#else
  #define BigRangeFlags ""
#endif

#endif // SHADER_VARS_H
