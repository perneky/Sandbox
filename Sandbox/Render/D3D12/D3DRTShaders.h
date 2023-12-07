#pragma once

#include "../RTShaders.h"

class D3DResource;

class D3DRTShaders : public RTShaders
{
  friend class D3DDevice;

public:
  ~D3DRTShaders();

  D3D12_GPU_VIRTUAL_ADDRESS_RANGE            GetRayGenerationShaderRecord() const;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetMissShaderTable() const;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE GetHitGroupTable() const;

  ID3D12RootSignature* GetD3DRootSignature();
  ID3D12StateObject*   GetD3DStateObject();

private:
  D3DRTShaders( D3DDevice& d3dDevice
              , CommandList& commandList
              , const eastl::vector< uint8_t >& rootSignatureShaderBinary
              , const eastl::vector< uint8_t >& shaderBinary
              , const wchar_t* rayGenEntryName
              , const wchar_t* missEntryName
              , const wchar_t* anyHitEntryName
              , const wchar_t* closestHitEntryName
              , int attributeSize
              , int payloadSize
              , int maxRecursionDepth );

  D3D12_GPU_VIRTUAL_ADDRESS_RANGE            rayGenerationShaderRecord;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE missShaderTable;
  D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE hitGroupTable;

  eastl::unique_ptr< D3DResource > shaderTable;

  CComPtr< ID3D12RootSignature > d3dRootSignature;
  CComPtr< ID3D12StateObject > d3dStateObject;
  CComPtr< ID3D12StateObjectProperties > d3dRaytracingPipeline;

  void* rayGenEntry   = nullptr;
  void* missEntry     = nullptr;
  void* hitGroupEntry = nullptr;

  eastl::vector< uint8_t > cache;

  int oneShaderSize = -1;
};