#pragma once

#ifndef SHADER_STRUCTURES_HLSLI
#define SHADER_STRUCTURES_HLSLI

#ifdef __cplusplus
# pragma pack( push, 1 )
#endif // __cplusplus

static const uint tileSize = 64 * 1024;

enum class VRSBlock : uint
{
  _1x1,
  _1x2,
  _2x1,
  _2x2,
};

enum class LightType : uint
{
  Directional,
  Point,
  Spot,
};

struct ExposureBuffer
{
  float exposure;
  float invExposure;
  float targetExposure;
  float weightedHistAvg;
  float minLog;
  float maxLog;
  float logRange;
  float invLogRange;
};

struct HaltonSequence
{
  static const uint HaltonSequenceLength = 511;

  float4 values[ HaltonSequenceLength ];
};

/////////////////////////////////////////////
// Scene representation on the GPU.
// There is a buffer for all nodes, one for all meshes and one for all cameras.

static const uint InvalidSlot = 0xFFFFFFFFU;

struct NodeSlot
{
  matrix worldTransform;
  uint   firstMeshSlot;
  uint   cameraSlot;
  uint   lightSlot;
  uint   nextSiblingSlot;
  uint   firstChildSlot;
  uint   padding[ 3 ];
};

// Mesh slot is a linked list, containing all meshes for a given node.
// The nextSlotIndex is the index of the next mesh for the node, or -1 if it is the last.
struct MeshSlot
{
  float4 aabbCenter;
  float4 aabbExtents;
  half4  randomValues;
  uint   ibIndex;
  uint   vbIndex;
  uint   indexCount;
  uint   materialIndex;
  uint   nextSlotIndex;
};

struct CameraSlot
{
  matrix projTransform;
};

struct MaterialSlot
{
  enum Flags : uint32_t
  {
    TwoSided    = 0x1,
    AlphaTested = 0x2,
    Translucent = 0x4,
    FlipWinding = 0x8,
  };

  half4 albedo;
  half2 roughness_metallic;
  uint  flags;
  int   albedoTextureIndex;
  int   albedoTextureRefIndex;
  int   normalTextureIndex;
  int   roughnessTextureIndex;
  int   metallicTextureIndex;
};

struct LightSlot
{
  half4     color;
  half4     attenuation; // constant, linear, quadratic
  half2     theta_phi;
  uint      castShadow;
  uint      scatterShadow;
  LightType type;
};

struct ModelMetaSlot
{
  uint materialIndex;
  uint indexBufferIndex;
  uint vertexBufferIndex;
  uint padding;
};

/////////////////////////////////////////////
// The scene is processed on the GPU, and it fills a buffer of IndirectRender, and one FrameParams.

struct IndirectRender
{
  float4x4 worldTransform;
  half4    randomValues;
  uint     ibIndex;
  uint     vbIndex;
  uint     materialIndex;
  uint     modelId;

  uint     vertexCountPerInstance;
  uint     instanceCount;
  uint     startVertexLocation;
  uint     startInstanceLocation;
};

struct FrameParams
{
  matrix vpTransform;
  matrix invVpTransform;
  matrix vpTransformNoJitter;
  matrix prevVPTransformNoJitter;
  matrix viewTransform;
  matrix projTransform;
  matrix invProjTransform;
  float4 cameraPosition;
  float4 cameraDirection;
  float4 giProbeOrigin;
  uint4  giProbeCount;
  uint2  rendererSize;
  float2 rendererSizeF;
  float2 invRendererSize;
  uint   lightCount;
  uint   feedbackPhase;
  uint   frameIndex;
  float  frameIndexF;
};

struct LightParams
{
  float4    origin;
  half4     direction;
  half4     color;
  half4     attenuation; // constant, linear, quadratic
  half2     theta_phi;
  uint      castShadow;
  uint      scatterShadow;
  LightType type;
};

enum class AlphaMode : uint
{
  Opaque,
  OneBitAlpha,
  Translucent,
};

struct VertexFormat
{
  half4 position;
  half4 tangent;
  half4 bitangent;
  half4 normal;
  half2 texcoord;
};

struct Sky
{
  float4 sunPosition;
  float  rayleigh;
  float  turbidity;
  float  mieCoefficient;
  float  vSunfade;
  float  mieDirectionalG;
  float  exposure;
};

struct ShadowPayload
{
  float3 color;
  float  distance;
};

struct ReflectionPayload
{
  float3 color;
  float  distance;
};

struct AOPayload
{
  float t;
};

static const float3 cubeLookAt[] =
{
  float3(  1,  0,  0 ),
  float3( -1,  0,  0 ),
  float3(  0,  1,  0 ),
  float3(  0, -1,  0 ),
  float3(  0,  0,  1 ),
  float3(  0,  0, -1 ),
};

static const float3 cubeUpDir[] =
{
  float3( 0,  1,  0 ),
  float3( 0,  1,  0 ),
  float3( 0,  0, -1 ),
  float3( 0,  0,  1 ),
  float3( 0,  1,  0 ),
  float3( 0,  1,  0 ),
};

#ifdef __cplusplus
# pragma pack( pop )
#endif // __cplusplus

#endif // SHADER_STRUCTURES_HLSLI