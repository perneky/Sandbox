#pragma once

#include "../Resource.h"
#include "AllocatedResource.h"
#include "D3DTileHeap.h"

class D3DDescriptorHeap;
class D3DDevice;
class D3DCommandQueue;

class D3DResource : public Resource
{
  friend class D3DCommandList;

public:
  using HeapAllocator = eastl::function< TileHeap::Allocation( Device& device, CommandList& commandList ) >;

  D3DResource( AllocatedResource&& allocation, ResourceState initialState );
  D3DResource( D3D12MA::Allocation* allocation, ResourceState initialState );
  D3DResource( D3DDevice& device, ResourceType resourceType, HeapType heapType, bool unorderedAccess, int size, int elementSize, const wchar_t* debugName );

  ~D3DResource();

  void                AttachResourceDescriptor( ResourceDescriptorType type, eastl::unique_ptr< ResourceDescriptor > descriptor ) override;
  ResourceDescriptor* GetResourceDescriptor( ResourceDescriptorType type ) override;
  void                RemoveResourceDescriptor( ResourceDescriptorType type ) override;
  void                RemoveAllResourceDescriptors() override;

  ResourceState GetCurrentResourceState() const override;

  ResourceType GetResourceType() const override;
  bool IsUploadResource() const override;

  int GetBufferSize() const override;

  int GetTextureWidth() const override;
  int GetTextureHeight() const override;
  int GetTextureDepthOrArraySize() const override;
  int GetTextureMipLevels() const override;
  int GetTextureTileWidth() const override;
  int GetTextureTileHeight() const override;
  PixelFormat GetTexturePixelFormat() const override;

  uint64_t GetVirtualAllocationSize() const override;
  uint64_t GetPhysicalAllocationSize() const override;

  void* Map() override;
  void  Unmap() override;

  void UploadLoadedTiles( Device& device, CommandQueue& copyQueue, CommandList& commandList ) override;

  void EndFeedback( CommandQueue& graphicsQueue, CommandQueue& copyQueue, Device& device, CommandList& commandList, uint64_t fence, uint64_t frameNo, int globalFeedback ) override;

  FileLoaderFile* GetLoader() override;

  void SetupForStreaming( eastl::unique_ptr< Resource >&& feedbackTexture
                        , eastl::unique_ptr< FileLoaderFile >&& fileHandle
                        , int firstMipLevelPosition
                        , HeapAllocator heapAllocator );

  ID3D12Resource*             GetD3DResource();
  D3D12_VERTEX_BUFFER_VIEW    GetD3DVertexBufferView();
  D3D12_INDEX_BUFFER_VIEW     GetD3DIndexBufferView();
  D3D12_GPU_VIRTUAL_ADDRESS   GetD3DGPUVirtualAddress();

protected:
  struct TileStats
  {
    int tx;
    int ty;
    int mip;

    TileHeap::Allocation allocation;

    uint64_t lastUsedFrame = 0;
  };

  int mipLevels = -1;
  int width     = -1;
  int height    = -1;

  bool ManageTile( Device& device
                 , CommandQueue& graphicsQueue
                 , CommandQueue& copyQueue
                 , CommandList& commandList
                 , int tx
                 , int ty
                 , int mipLevel
                 , uint64_t frameNo
                 , int globalFeedback );

  void DropTile( Device& device, CommandQueue& copyQueue, CommandList& commandList, TileStats& stats );

  void UpdateTile( Device& device, CommandQueue& copyQueue, CommandList& commandList, const TileStats& stats );

  ResourceType resourceType;
  bool         isUploadResource;

  ResourceState     resourceState;
  AllocatedResource d3dResource;
  AllocatedResource d3dFeedbackResolved;
  int               descriptorIndex;
  
  eastl::unique_ptr< ResourceDescriptor > resourceDescriptors[ 9 ];

  eastl::function< void() > onDelete;

  union
  {
    D3D12_VERTEX_BUFFER_VIEW vbView;
    D3D12_INDEX_BUFFER_VIEW  ibView;
  };

  HeapAllocator heapAllocator;
  eastl::vector< eastl::vector< TileStats > > tileMapping;
  eastl::atomic< int > allocatedTileCount = 0;
  int firstMipLevelPosition = -1;

  eastl::unique_ptr< FileLoaderFile > streamingFileHandle = nullptr;

  uint64_t pendingResolveFence = 0;

  eastl::unique_ptr< Resource > feedbackTexture;

  D3D12_PACKED_MIP_INFO    packedMipInfo     = {};
  D3D12_TILE_SHAPE         tileShape         = {};
  D3D12_SUBRESOURCE_TILING subresourceTiling = {};

  eastl::wstring debugName;
};