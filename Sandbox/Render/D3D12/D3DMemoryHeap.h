#pragma once

#include "../MemoryHeap.h"

class D3DMemoryHeap : public MemoryHeap
{
  friend class D3DDevice;

public:
  ~D3DMemoryHeap();

  ID3D12Heap* GetD3DHeap();

  uint64_t alloc( uint64_t size ) override;
  void free( uint64_t address ) override;

private:
  D3DMemoryHeap( D3DDevice& device, uint64_t size, const wchar_t* debugName );

  CComPtr< ID3D12Heap > d3dHeap;

  eastl::vector< uint64_t > allocations;
  eastl::vector< uint64_t > allocationLinks;

  uint64_t size;

  CRITICAL_SECTION allocationLock;
};