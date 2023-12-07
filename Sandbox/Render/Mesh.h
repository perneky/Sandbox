#pragma once

#include "Render/ShaderStructures.h"
#include "Types.h"

struct Resource;
struct CommandList;
struct Device;
struct RTBottomLevelAccelerator;

enum class AlphaModeCB : int;

class Mesh
{
  friend struct MeshHelper;

public:
  Mesh( CommandList& commandList
      , eastl::unique_ptr< Resource >&& vertexBuffer
      , eastl::unique_ptr< Resource >&& indexBuffer
      , int vertexCount, int indexCount
      , int materialIndex
      , bool opaque
      , Resource& modelMetaBuffer
      , int modelMetaIndex
      , const BoundingBox& aabb
      , const char* debugName );
  ~Mesh();

  bool HasTranslucent() const;
  int  GetVertexCount() const;
  int  GetIndexCount() const;
  int  GetMaterialIndex() const;

  Resource& GetVertexBufferResource();
  Resource& GetIndexBufferResource();
  
  RTBottomLevelAccelerator& GetRTBottomLevelAccelerator();

  int GetVertexBufferSlot() const;
  int GetIndexBufferSlot() const;

  const BoundingBox& GetAABB() const;

  void Dispose( CommandList& commandList );

private:
  struct Batch;

  using IndexFormat = uint16_t;

  eastl::unique_ptr< Resource > vertexBuffer;
  eastl::unique_ptr< Resource > indexBuffer;
  
  eastl::unique_ptr< RTBottomLevelAccelerator > blas;

  int vertexCount = 0;
  int indexCount  = 0;

  int vbSlot = -1;
  int ibSlot = -1;

  int materialIndex = -1;

  BoundingBox aabb;

  eastl::wstring debugName;
};