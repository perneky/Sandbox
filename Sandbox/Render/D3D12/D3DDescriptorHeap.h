#pragma once

#include "../DescriptorHeap.h"

class D3DDescriptorHeap : public DescriptorHeap
{
  friend class D3DDevice;

public:
  ~D3DDescriptorHeap();

  eastl::unique_ptr< ResourceDescriptor > RequestDescriptorFromSlot( Device& device, ResourceDescriptorType type, int slot, Resource& resource, int bufferElementSize, int mipLevel = 0 ) override;
  eastl::unique_ptr< ResourceDescriptor > RequestDescriptorAuto( Device& device, ResourceDescriptorType type, int base, Resource& resource, int bufferElementSize, int mipLevel = 0 ) override;

  int GetDescriptorSize() const override;

  void FreeDescriptor( int index );

  void RequestDescriptor( int slot, D3D12_CPU_DESCRIPTOR_HANDLE& d3dCPUHandle, D3D12_CPU_DESCRIPTOR_HANDLE& d3dShaderVisibleCPUHandle, D3D12_GPU_DESCRIPTOR_HANDLE& d3dShaderVisibleGPUHandle );

  ID3D12DescriptorHeap* GetD3DCpuHeap();
  ID3D12DescriptorHeap* GetD3DGpuHeap();

  D3D12_DESCRIPTOR_HEAP_TYPE GetD3DHeapType();

private:
  D3DDescriptorHeap( D3DDevice& device, int descriptorCount, D3D12_DESCRIPTOR_HEAP_TYPE heapType, const wchar_t* debugName );

  eastl::unique_ptr< ResourceDescriptor > RequestDescriptor( Device& device, ResourceDescriptorType type, int base, int slot, Resource& resource, int bufferElementSize, int mipLevel );

  CComPtr< ID3D12DescriptorHeap > d3dCPUVisibleHeap;
  CComPtr< ID3D12DescriptorHeap > d3dGPUVisibleHeap;

  CComPtr< ID3D12Resource > d3dDummySRVBuffer;
  CComPtr< ID3D12Resource > d3dDummySRVTexture;
  CComPtr< ID3D12Resource > d3dDummyUAVTexture;

  eastl::recursive_mutex descriptorLock;
  eastl::vector_set< int > freeDescriptors;

  size_t handleSize = 0;

  D3D12_DESCRIPTOR_HEAP_TYPE heapType;
};