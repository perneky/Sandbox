#include "Mesh.h"
#include "Device.h"
#include "Resource.h"
#include "ResourceDescriptor.h"
#include "RenderManager.h"
#include "CommandList.h"
#include "DescriptorHeap.h"
#include "Utils.h"
#include "RTBottomLevelAccelerator.h"
#include "ShaderStructures.h"
#include "ShaderValues.h"
#include "ModelFeatures.h"
#include "Common/Files.h"

using namespace DirectX;

Mesh::Mesh( CommandList& commandList
          , eastl::unique_ptr< Resource >&& vertexBufferIn
          , eastl::unique_ptr< Resource >&& indexBufferIn
          , int vertexCount
          , int indexCount
          , int materialIndex
          , bool opaque
          , Resource& modelMetaBuffer
          , int modelMetaIndex
          , const BoundingBox& aabb
          , const char* debugName )
: vertexBuffer ( eastl::forward< eastl::unique_ptr< Resource > >( vertexBufferIn ) )
, indexBuffer  ( eastl::forward< eastl::unique_ptr< Resource > >( indexBufferIn  ) )
, vertexCount  ( vertexCount )
, indexCount   ( indexCount  )
, materialIndex( materialIndex )
, aabb         ( aabb )
, debugName    ( W( debugName ) )
{
  auto& device = RenderManager::GetInstance().GetDevice();

  auto vbDesc = device.GetShaderResourceHeap().RequestDescriptorAuto( device, ResourceDescriptorType::ShaderResourceView, SceneBufferResourceBaseSlot, *vertexBuffer, sizeof( VertexFormat ) );
  vbSlot = vbDesc->GetSlot();
  vertexBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( vbDesc ) );

  auto ibDesc = device.GetShaderResourceHeap().RequestDescriptorAuto( device, ResourceDescriptorType::ShaderResourceView, SceneBufferResourceBaseSlot, *indexBuffer, sizeof( uint32_t ) );
  ibSlot = ibDesc->GetSlot();
  indexBuffer->AttachResourceDescriptor( ResourceDescriptorType::ShaderResourceView, eastl::move( ibDesc ) );

  ModelMetaSlot modelMetaSlot;
  modelMetaSlot.materialIndex     = materialIndex;
  modelMetaSlot.indexBufferIndex  = ibSlot - SceneBufferResourceBaseSlot;
  modelMetaSlot.vertexBufferIndex = vbSlot - SceneBufferResourceBaseSlot;

  commandList.UpdateBufferRegion( CreateBufferFromData( &modelMetaSlot, 1, ResourceType::Buffer, device, commandList, L"modelMetaSlot" ), modelMetaBuffer, sizeof( ModelMetaSlot ) * modelMetaIndex );

  blas = device.CreateRTBottomLevelAccelerator( commandList, *vertexBuffer, vertexCount, sizeof( uint16_t ) * 8, sizeof( VertexFormat ), *indexBuffer, sizeof( uint32_t ) * 8, indexCount, modelMetaIndex, opaque, false, false );
}

Mesh::~Mesh()
{
}

bool Mesh::HasTranslucent() const
{
  return false;
}

int Mesh::GetVertexCount() const
{
  return vertexCount;
}

int Mesh::GetIndexCount() const
{
  return indexCount;
}

int Mesh::GetMaterialIndex() const
{
  return materialIndex;
}

Resource& Mesh::GetVertexBufferResource()
{
  return *vertexBuffer;
}

Resource& Mesh::GetIndexBufferResource()
{
  return *indexBuffer;
}

RTBottomLevelAccelerator& Mesh::GetRTBottomLevelAccelerator()
{
  return *blas;
}

int Mesh::GetVertexBufferSlot() const
{
  return vbSlot;
}

int Mesh::GetIndexBufferSlot() const
{
  return ibSlot;
}

const BoundingBox& Mesh::GetAABB() const
{
  return aabb;
}

void Mesh::Dispose( CommandList& commandList )
{
  commandList.HoldResource( eastl::move( vertexBuffer ) );
  commandList.HoldResource( eastl::move( indexBuffer ) );
}
