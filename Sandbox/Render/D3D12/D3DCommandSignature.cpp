#include "D3DCommandSignature.h"
#include "D3DPipelineState.h"
#include "D3DDevice.h"

static D3D12_INDIRECT_ARGUMENT_TYPE Convert( CommandSignatureDesc::Argument::Type type )
{
  switch ( type )
  {
  case CommandSignatureDesc::Argument::Type::Draw:                return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
  case CommandSignatureDesc::Argument::Type::DrawIndexed:         return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
  case CommandSignatureDesc::Argument::Type::Dispatch:            return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
  case CommandSignatureDesc::Argument::Type::VertexBufferView:    return D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
  case CommandSignatureDesc::Argument::Type::IndexBufferView:     return D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
  case CommandSignatureDesc::Argument::Type::Constant:            return D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
  case CommandSignatureDesc::Argument::Type::ConstantBufferView:  return D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
  case CommandSignatureDesc::Argument::Type::ShaderResourceView:  return D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
  case CommandSignatureDesc::Argument::Type::UnorderedAccessView: return D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;

  default:
    assert( false );
    return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
  }
}

D3DCommandSignature::D3DCommandSignature( CommandSignatureDesc& desc, D3DPipelineState& pipelineState, D3DDevice& device )
{
  auto d3dDevice = device.GetD3DDevice();
    
  D3D12_COMMAND_SIGNATURE_DESC csDesc;
  ZeroObject( csDesc );

  eastl::vector< D3D12_INDIRECT_ARGUMENT_DESC > args;

  for ( auto& src : desc.arguments )
  {
    args.emplace_back();
    auto& dst = args.back();

    dst.Type = Convert( src.type );

    switch ( src.type )
    {
    case CommandSignatureDesc::Argument::Type::Draw:
    case CommandSignatureDesc::Argument::Type::DrawIndexed:
    case CommandSignatureDesc::Argument::Type::Dispatch:
    case CommandSignatureDesc::Argument::Type::IndexBufferView:
      break;
    case CommandSignatureDesc::Argument::Type::VertexBufferView:
      dst.VertexBuffer.Slot = src.vertexBuffer.slot;
      break;
    case CommandSignatureDesc::Argument::Type::Constant:
      dst.Constant.DestOffsetIn32BitValues = src.constant.destOffsetIn32BitValues;
      dst.Constant.Num32BitValuesToSet     = src.constant.num32BitValuesToSet;
      dst.Constant.RootParameterIndex      = src.constant.rootParameterIndex;
      break;
    case CommandSignatureDesc::Argument::Type::ConstantBufferView:
      dst.ConstantBufferView.RootParameterIndex = src.constantBufferView.rootParameterIndex;
      break;
    case CommandSignatureDesc::Argument::Type::ShaderResourceView:
      dst.ShaderResourceView.RootParameterIndex = src.shaderResourceView.rootParameterIndex;
      break;
    case CommandSignatureDesc::Argument::Type::UnorderedAccessView:
      dst.UnorderedAccessView.RootParameterIndex = src.unorderedAccessView.rootParameterIndex;
      break;

    default:
      assert( false );
    }
  }

  csDesc.ByteStride       = desc.stride;
  csDesc.NumArgumentDescs = int( desc.arguments.size() );
  csDesc.pArgumentDescs   = args.data();

  d3dDevice->CreateCommandSignature( &csDesc, pipelineState.GetD3DRootSignature(), IID_PPV_ARGS( &d3dCommandSignature ) );
}

D3DCommandSignature::~D3DCommandSignature()
{
}

ID3D12CommandSignature* D3DCommandSignature::GetD3DCommandSignature()
{
  return d3dCommandSignature;
}