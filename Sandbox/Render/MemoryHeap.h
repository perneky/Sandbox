#pragma once

struct MemoryHeap
{
  static constexpr uint64_t InvalidAllocation = 0xFFFFFFFFFFFFFFFFLLU;

  virtual ~MemoryHeap() = default;

  virtual uint64_t alloc( uint64_t size ) = 0;
  virtual void free( uint64_t address ) = 0;
};