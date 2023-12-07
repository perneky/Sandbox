#pragma once

#include "../CommandSignature.h"
#include "../Types.h"

class D3DPipelineState;

class D3DCommandSignature : public CommandSignature
{
  friend class D3DDevice;

public:
  ~D3DCommandSignature();

  ID3D12CommandSignature* GetD3DCommandSignature();

private:
  D3DCommandSignature( CommandSignatureDesc& desc, D3DPipelineState& pipelineState, D3DDevice& device );

  CComPtr< ID3D12CommandSignature > d3dCommandSignature;
};