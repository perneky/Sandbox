#pragma once

#include "Types.h"
#include "TileHeap.h"

struct Device;
struct CommandList;
struct CommandQueue;
struct Resource;
struct TFFHeader;

struct FileLoaderFile
{
  using OnTileLoadAction = eastl::function< void( Device& device, CommandQueue& copyQueue, CommandList& commandList ) >;

  virtual ~FileLoaderFile() = default;

  virtual void LoadPackedMipTail( Device& device
                                , CommandList& commandList
                                , Resource& resource
                                , const TFFHeader& tffHeader
                                , int blockSize ) = 0;

  virtual void LoadSingleTile( Device& device
                             , CommandList& commandList
                             , TileHeap::Allocation allocation
                             , int byteOffset
                             , OnTileLoadAction onTileLoadAction ) = 0;

  virtual void UploadLoadedTiles( Device& device, CommandQueue& copyQueue, CommandList& commandList ) = 0;
};

struct FileLoaderQueue
{
  virtual ~FileLoaderQueue() = default;

  virtual void Enqueue( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat, eastl::unique_ptr< Resource >& uploadResource ) = 0;
};

struct FileLoader
{
  virtual ~FileLoader() = default;

  virtual eastl::unique_ptr< FileLoaderFile > OpenFile( const eastl::wstring& path ) = 0;
};

eastl::unique_ptr< FileLoader > CreateFileLoader( Device& device );
