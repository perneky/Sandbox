#pragma once

struct CommandList;
struct Resource;
struct RTTopLevelAccelerator;

struct GIVolume
{
public:
  struct UpdatePackage
  {
    Resource& frameParamsBuffer;
    Resource& modelMetaBuffer;
    Resource& materialBuffer;
    Resource& lightParamsBuffer;
    RTTopLevelAccelerator& tlas;
  };

  virtual ~GIVolume() = default;

  static eastl::unique_ptr< GIVolume > Create( CommandList& commandList, const BoundingBox& bounds, float probeSpacing );

  virtual void UpdateGI( CommandList& commandList, const UpdatePackage& updatePackage ) = 0;
  virtual void TraceGI( CommandList& commandList, const UpdatePackage& updatePackage ) = 0;

  virtual void DebugRenderProbes( CommandList& commandList, Resource& frameParams ) = 0;
};