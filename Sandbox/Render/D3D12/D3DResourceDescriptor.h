#pragma once

#include "../ResourceDescriptor.h"
#include "../Types.h"
#include "Common/Finally.h"

class D3DResource;

class D3DResourceDescriptor : public ResourceDescriptor
{
  friend class D3DDescriptorHeap;
  friend class D3DSwapchain;
  friend class D3DDevice;

public:
  virtual ~D3DResourceDescriptor();

  int GetSlot() const override;

  D3D12_CPU_DESCRIPTOR_HANDLE GetD3DCPUHandle() const;
  D3D12_CPU_DESCRIPTOR_HANDLE GetD3DShaderVisibleCPUHandle() const;
  D3D12_GPU_DESCRIPTOR_HANDLE GetD3DShaderVisibleGPUHandle() const;

private:
  D3DResourceDescriptor( D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle );
  D3DResourceDescriptor( D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle
                       , D3D12_CPU_DESCRIPTOR_HANDLE d3dShaderVisibleCPUHandle
                       , D3D12_GPU_DESCRIPTOR_HANDLE d3dShaderVisibleGPUHandle
                       , D3DDescriptorHeap& heap
                       , int slot );
  D3DResourceDescriptor( D3DDevice& device
                       , D3DDescriptorHeap& heap
                       , ResourceDescriptorType type
                       , int slot
                       , D3DResource& resource
                       , int mipLevel
                       , int bufferElementSize
                       , D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle
                       , D3D12_CPU_DESCRIPTOR_HANDLE d3dShaderVisibleCPUHandle
                       , D3D12_GPU_DESCRIPTOR_HANDLE d3dShaderVisibleGPUHandle );

  int                         slot = -1;
  D3D12_CPU_DESCRIPTOR_HANDLE d3dCPUHandle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE d3dShaderVisibleCPUHandle = {};
  D3D12_GPU_DESCRIPTOR_HANDLE d3dShaderVisibleGPUHandle = {};
  Finally                     unregister;
};