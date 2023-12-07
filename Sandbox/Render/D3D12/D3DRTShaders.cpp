#include "D3DRTShaders.h"
#include "D3DDevice.h"
#include "D3DResource.h"
#include "../Utils.h"

D3DRTShaders::D3DRTShaders( D3DDevice& d3dDevice
                          , CommandList& commandList
                          , const eastl::vector< uint8_t >& rootSignatureShaderBinary
                          , const eastl::vector< uint8_t >& shaderBinary
                          , const wchar_t* rayGenEntryName
                          , const wchar_t* missEntryName
                          , const wchar_t* anyHitEntryName
                          , const wchar_t* closestHitEntryName
                          , int attributeSize
                          , int payloadSize
                          , int maxRecursionDepth )
{
  assert( rayGenEntryName );

  auto hr = d3dDevice.GetD3DDevice()->CreateRootSignature(0, rootSignatureShaderBinary.data(), rootSignatureShaderBinary.size(), IID_PPV_ARGS( &d3dRootSignature ) );
  assert( SUCCEEDED( hr ) );

  eastl::vector< D3D12_STATE_SUBOBJECT > subobjects;
  subobjects.reserve( 20 );

  // Specify the shader binaries and entry points
  eastl::vector< D3D12_EXPORT_DESC > exports;
  if ( rayGenEntryName )
  {
    D3D12_EXPORT_DESC exp;
    exp.Name           = rayGenEntryName;
    exp.ExportToRename = nullptr;
    exp.Flags          = D3D12_EXPORT_FLAG_NONE;
    exports.emplace_back( exp );
  }
  if ( missEntryName )
  {
    D3D12_EXPORT_DESC exp;
    exp.Name           = missEntryName;
    exp.ExportToRename = nullptr;
    exp.Flags          = D3D12_EXPORT_FLAG_NONE;
    exports.emplace_back( exp );
  }
  if ( anyHitEntryName )
  {
    D3D12_EXPORT_DESC exp;
    exp.Name           = anyHitEntryName;
    exp.ExportToRename = nullptr;
    exp.Flags          = D3D12_EXPORT_FLAG_NONE;
    exports.emplace_back( exp );
  }
  if ( closestHitEntryName )
  {
    D3D12_EXPORT_DESC exp;
    exp.Name           = closestHitEntryName;
    exp.ExportToRename = nullptr;
    exp.Flags          = D3D12_EXPORT_FLAG_NONE;
    exports.emplace_back( exp );
  }

  D3D12_DXIL_LIBRARY_DESC shaderBinaryDesc;
  shaderBinaryDesc.DXILLibrary.pShaderBytecode = shaderBinary.data();
  shaderBinaryDesc.DXILLibrary.BytecodeLength  = shaderBinary.size();
  shaderBinaryDesc.NumExports                  = UINT( exports.size() );
  shaderBinaryDesc.pExports                    = exports.data();

  D3D12_STATE_SUBOBJECT dxilSubobject;
  dxilSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
  dxilSubobject.pDesc = &shaderBinaryDesc;
  subobjects.emplace_back( dxilSubobject );

  // Specify the hit group and entry points
  D3D12_HIT_GROUP_DESC hitGroupDesc;
  hitGroupDesc.Type                     = D3D12_HIT_GROUP_TYPE_TRIANGLES;
  hitGroupDesc.AnyHitShaderImport       = anyHitEntryName;
  hitGroupDesc.ClosestHitShaderImport   = closestHitEntryName;
  hitGroupDesc.HitGroupExport           = L"HitGroup";
  hitGroupDesc.IntersectionShaderImport = nullptr;
  
  D3D12_STATE_SUBOBJECT hitGroupSubobject;
  hitGroupSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
  hitGroupSubobject.pDesc = &hitGroupDesc;
  subobjects.emplace_back( hitGroupSubobject );

  // Specify the root signature
  D3D12_GLOBAL_ROOT_SIGNATURE rootSignature;
  rootSignature.pGlobalRootSignature = d3dRootSignature;

  D3D12_STATE_SUBOBJECT rootSignatureSubobject;
  rootSignatureSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
  rootSignatureSubobject.pDesc = &rootSignature;
  subobjects.emplace_back( rootSignatureSubobject );

  // Link the root signature to the entry points
  eastl::vector< const wchar_t* > associationExportNames;
  associationExportNames.emplace_back( rayGenEntryName );
  if ( missEntryName )
    associationExportNames.emplace_back( missEntryName );
  if ( anyHitEntryName )
    associationExportNames.emplace_back( anyHitEntryName );
  if ( closestHitEntryName )
    associationExportNames.emplace_back( closestHitEntryName );

  D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION raygenAssociation;
  raygenAssociation.pSubobjectToAssociate = &subobjects.back(); // rootSignatureSubobject
  raygenAssociation.NumExports            = UINT( associationExportNames.size() );
  raygenAssociation.pExports              = associationExportNames.data();

  D3D12_STATE_SUBOBJECT raygenRootSignatureAssociationSubobject;
  raygenRootSignatureAssociationSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
  raygenRootSignatureAssociationSubobject.pDesc = &raygenAssociation;
  subobjects.emplace_back( raygenRootSignatureAssociationSubobject );

  // Specify the payload size
  D3D12_RAYTRACING_SHADER_CONFIG shaderConfig;
  shaderConfig.MaxAttributeSizeInBytes = UINT( attributeSize );
  shaderConfig.MaxPayloadSizeInBytes   = UINT( payloadSize );

  D3D12_STATE_SUBOBJECT payloadSizeSubobject;
  payloadSizeSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
  payloadSizeSubobject.pDesc = &shaderConfig;
  subobjects.emplace_back( payloadSizeSubobject );

  D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION payloadAssociation;
  payloadAssociation.pSubobjectToAssociate = &subobjects.back(); // payloadSizeSubobject;
  payloadAssociation.NumExports            = UINT( associationExportNames.size() );
  payloadAssociation.pExports              = associationExportNames.data();

  D3D12_STATE_SUBOBJECT payloadAssociationSubobject;
  payloadAssociationSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
  payloadAssociationSubobject.pDesc = &payloadAssociation;
  subobjects.emplace_back( payloadAssociationSubobject );

  // Specify max recursion depth
  D3D12_RAYTRACING_PIPELINE_CONFIG rtPipelineConfig;
  rtPipelineConfig.MaxTraceRecursionDepth = maxRecursionDepth;

  D3D12_STATE_SUBOBJECT recursionSubobject;
  recursionSubobject.Type  = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
  recursionSubobject.pDesc = &rtPipelineConfig;
  subobjects.emplace_back( recursionSubobject );

  // Create state object
  D3D12_STATE_OBJECT_DESC desc;
  desc.Type          = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
  desc.NumSubobjects = UINT( subobjects.size() );
  desc.pSubobjects   = subobjects.data();

  d3dDevice.GetD3DDevice()->CreateStateObject( &desc, IID_PPV_ARGS( &d3dStateObject ) );

  // Create the shader table
  d3dStateObject.QueryInterface( &d3dRaytracingPipeline );
  assert( d3dRaytracingPipeline );

  oneShaderSize = align_to( D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );

  int shaderSize = oneShaderSize; // raygen
  shaderSize += missEntryName ? oneShaderSize : 0;
  shaderSize += ( anyHitEntryName || closestHitEntryName ) ? oneShaderSize : 0;

  shaderSize = align_to( 256, shaderSize );

  cache.resize( shaderSize );

  {
    auto buffer = d3dDevice.CreateBuffer( ResourceType::Buffer, HeapType::Default, false, shaderSize, shaderSize, L"RTShaderTable" );
    shaderTable.reset( static_cast< D3DResource* >( buffer.release() ) );
  }

  rayGenEntry = d3dRaytracingPipeline->GetShaderIdentifier( rayGenEntryName );
  assert( rayGenEntry );

  if ( missEntryName )
  {
    missEntry = d3dRaytracingPipeline->GetShaderIdentifier( missEntryName );
    assert( missEntry );
  }

  if ( anyHitEntryName || closestHitEntryName )
  {
    hitGroupEntry = d3dRaytracingPipeline->GetShaderIdentifier( L"HitGroup" );
    assert( hitGroupEntry );
  }

  auto d3dResource = shaderTable->GetD3DResource();

  rayGenerationShaderRecord.StartAddress = d3dResource->GetGPUVirtualAddress();
  rayGenerationShaderRecord.SizeInBytes  = oneShaderSize;

  missShaderTable.StartAddress  = missEntryName ? rayGenerationShaderRecord.StartAddress + rayGenerationShaderRecord.SizeInBytes : 0;
  missShaderTable.SizeInBytes   = missEntryName ? oneShaderSize : 0;
  missShaderTable.StrideInBytes = missEntryName ? missShaderTable.SizeInBytes : 0;

  hitGroupTable.StartAddress  = (anyHitEntryName || closestHitEntryName) ? rayGenerationShaderRecord.StartAddress + rayGenerationShaderRecord.SizeInBytes + missShaderTable.SizeInBytes : 0;
  hitGroupTable.SizeInBytes   = (anyHitEntryName || closestHitEntryName) ? oneShaderSize : 0;
  hitGroupTable.StrideInBytes = (anyHitEntryName || closestHitEntryName) ? hitGroupTable.SizeInBytes : 0;

  uint8_t* data = cache.data();

  memcpy_s( data, oneShaderSize, rayGenEntry, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
  data += oneShaderSize;

  if ( missEntry )
  {
    memcpy_s( data, oneShaderSize, missEntry, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
    data += oneShaderSize;
  }

  if ( hitGroupEntry )
  {
    memcpy_s( data, oneShaderSize, hitGroupEntry, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES );
    data += oneShaderSize;
  }

  auto uploadBuffer = RenderManager::GetInstance().GetUploadBufferForResource( *shaderTable );
  commandList.UploadBufferResource( eastl::move( uploadBuffer ), *shaderTable, cache.data(), int( cache.size() ) );
}

D3DRTShaders::~D3DRTShaders()
{
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE D3DRTShaders::GetRayGenerationShaderRecord() const
{
  return rayGenerationShaderRecord;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE D3DRTShaders::GetMissShaderTable() const
{
  return missShaderTable;
}

D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE D3DRTShaders::GetHitGroupTable() const
{
  return hitGroupTable;
}

ID3D12RootSignature* D3DRTShaders::GetD3DRootSignature()
{
  return d3dRootSignature;
}

ID3D12StateObject* D3DRTShaders::GetD3DStateObject()
{
  return d3dStateObject;
}
