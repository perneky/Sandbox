#pragma once

#include "Render/Upscaling.h"

class Mesh;
class Node;
struct RTInstance;
struct RTShaders;
struct CommandList;
struct Resource;
struct ComputeShader;
struct NodeSlot;
struct MeshSlot;
struct CameraSlot;
struct LightSlot;
struct RTTopLevelAccelerator;
struct PipelineState;
struct CommandSignature;
struct CommandAllocator;
struct Denoiser;

enum class DebugOutput : uint32_t;

class Scene
{
public:
  Scene( CommandList& commandList, const wchar_t* hostFolder, int screenWidth, int screenHeight );
  ~Scene();

  void SetManualExposure( float exposure );

  void TearDown( CommandList* commandList );

  void Render( CommandAllocator& commandAllocator
             , CommandList& commandList
             , Resource& backBuffer
             , bool useTextureFeedback
             , bool freezeCulling
             , DebugOutput debugOutput );

  Node* FindNodeByName( const char* name );

  void OnNodeTransformChanged( CommandList& commandList, const Node& node );

  const eastl::wstring& GetError() const;

  void TearDownSceneBuffers( CommandList& commandList );
  void TearDownScreenSizeDependantTextures( CommandList& commandList );

  void OnScreenResize( CommandList& commandList, int width, int height );

  Upscaling::Quality GetUpscalingQuality() const;
  void SetUpscalingQuality( CommandList& commandList, int width, int height, Upscaling::Quality quality );

private:
  void BuildSceneBuffers( CommandList& commandList );

  void RecreateScrenSizeDependantTextures( CommandList& commandList, int width, int height );
  void CreateBRDFLUTTexture( CommandList& commandList );

  void SetupTriangleBuffers( CommandList& commandList, bool compute );

  void CullScene( CommandList& commandList, float jitterX, float jitterY, int targetWidth, int targetHeight, bool useTextureFeedback, bool freezeCulling );
  void ClearTexturesAndPrepareRendering( CommandList& commandList, Resource& renderTarget );
  void RenderSkyToCube( CommandList& commandList );
  void RenderDepth( CommandList& commandList );
  void RenderShadow( CommandList& commandList );
  void RenderAO( CommandList& commandList );
  void RenderGI( CommandList& commandList );
  void Denoise( CommandAllocator& commandAllocator, CommandList& commandList, float jitterX, float jitterY, bool showDenoiserDebugLayer );
  void RenderReflection( CommandList& commandList );
  void RenderDirectLighting( CommandList& commandList );
  void RenderTranslucent( CommandList& commandList );
  void RenderSky( CommandList& commandList );
  void PostProcessing( CommandList& commandList, Resource& backBuffer );
  void AdaptExposure( CommandList& commandList );
  void Upscale( CommandList& commandList, Resource& backBuffer );
  void RenderDebugLayer( CommandList& commandList, DebugOutput debugOutput );

  eastl::wstring error;

  eastl::vector< NodeSlot   > nodeSlots;
  eastl::vector< MeshSlot   > meshSlots;
  eastl::vector< CameraSlot > cameraSlots;
  eastl::vector< LightSlot  > lightSlots;
  eastl::vector< uint32_t   > rootNodeChildrenIndices;

  eastl::vector< eastl::unique_ptr< Mesh > > meshes;

  eastl::unique_ptr < RTTopLevelAccelerator > tlas;

  eastl::unique_ptr< Resource > nodeBuffer;
  eastl::unique_ptr< Resource > meshBuffer;
  eastl::unique_ptr< Resource > materialBuffer;
  eastl::unique_ptr< Resource > cameraBuffer;
  eastl::unique_ptr< Resource > lightBuffer;
  eastl::unique_ptr< Resource > rootNodeChildrenInidcesBuffer;

  eastl::unique_ptr< Resource > frameParamsBuffer;
  eastl::unique_ptr< Resource > lightParamsBuffer;
  eastl::unique_ptr< Resource > indirectOpaqueDrawBuffer;
  eastl::unique_ptr< Resource > indirectOpaqueTwoSidedDrawBuffer;
  eastl::unique_ptr< Resource > indirectOpaqueAlphaTestedDrawBuffer;
  eastl::unique_ptr< Resource > indirectOpaqueTwoSidedAlphaTestedDrawBuffer;
  eastl::unique_ptr< Resource > indirectTranslucentDrawBuffer;
  eastl::unique_ptr< Resource > indirectTranslucentTwoSidedDrawBuffer;
  eastl::unique_ptr< Resource > indirectDrawCountBuffer;
  eastl::unique_ptr< Resource > modelMetaBuffer;

  eastl::unique_ptr< Resource > lqColorTexture;
  eastl::unique_ptr< Resource > hqColorTexture;
  eastl::unique_ptr< Resource > depthTexture;

  eastl::unique_ptr< Resource > aoTexture;
  eastl::unique_ptr< Resource > reflectionTexture;
  eastl::unique_ptr< Resource > giTexture;

  eastl::unique_ptr< Resource > debugTexture;
  eastl::unique_ptr< Resource > motionVectorTexture;
  eastl::unique_ptr< Resource > textureMipTexture;
  eastl::unique_ptr< Resource > geometryIdsTexture;

  eastl::unique_ptr< Resource > exposureBuffer;
  eastl::unique_ptr< Resource > exposureOnlyBuffer;

  eastl::unique_ptr< Resource > bloomTextures[ 5 ][ 2 ];
  eastl::unique_ptr< Resource > lumaTexture;
  eastl::unique_ptr< Resource > histogramBuffer;

  eastl::unique_ptr< Resource > skyBuffer;
  eastl::unique_ptr< Resource > skyTexture;

  eastl::unique_ptr< Resource > shadowTexture;
  eastl::unique_ptr< Resource > shadowTransTexture;

  eastl::unique_ptr< Resource > specBRDFLUTTexture;

  eastl::unique_ptr< Resource > scramblingRankingTexture;
  eastl::unique_ptr< Resource > sobolTexture;

  eastl::unique_ptr< ComputeShader > prepareCullingShader;
  eastl::unique_ptr< ComputeShader > cullingShader;
  eastl::unique_ptr< ComputeShader > specBRDFLUTShader;

  eastl::unique_ptr< ComputeShader > blurShader;
  eastl::unique_ptr< ComputeShader > downsampleShader;
  eastl::unique_ptr< ComputeShader > downsample4Shader;
  eastl::unique_ptr< ComputeShader > downsampleMSAA4Shader;
  eastl::unique_ptr< ComputeShader > downsample4WLumaShader;

  eastl::unique_ptr< ComputeShader > extractBloomShader;
  eastl::unique_ptr< ComputeShader > downsampleBloomShader;
  eastl::unique_ptr< ComputeShader > blurBloomShader;
  eastl::unique_ptr< ComputeShader > upsampleBlurBloomShader;

  eastl::unique_ptr< ComputeShader > generateHistogramShader;
  eastl::unique_ptr< ComputeShader > adaptExposureShader;

  eastl::unique_ptr< RTShaders > traceGIShader;
  eastl::unique_ptr< RTShaders > traceAOShader;
  eastl::unique_ptr< RTShaders > traceShadowShader;
  eastl::unique_ptr< RTShaders > traceReflectionShader;

  eastl::unique_ptr< Node > rootNode;

  eastl::vector_map< const Node*, int > nodeToIndex;
  eastl::vector_map< int, const Node* > indexToNode;

  eastl::unique_ptr< Denoiser > denoiser;

  Resource* denoisedAOTexture = nullptr;
  Resource* denoisedShadowTexture = nullptr;
  Resource* denoisedReflectionTexture = nullptr;
  Resource* denoisedGITexture = nullptr;
  Resource* denoiserValidationTexture = nullptr;

  eastl::unique_ptr< Upscaling > upscaling;
  Upscaling::Quality           upscalingQuality = Upscaling::DefaultQuality;

  uint32_t frameCounter = 0;

  int cameraNodeIndex = -1;

  int instanceCount = 0;

  float manualExposure;
  float targetLuminance;
  float adaptationRate;
  float minExposure;
  float maxExposure;
  float bloomThreshold;
  float bloomStrength;

  void MarshallSceneToGPU( Node& sceneNode, int& instanceCount, Scene& scene );
  void MarshallSceneToRTInstances( Node& sceneNode, eastl::vector< RTInstance >& rtInstances, Scene& scene );
};
