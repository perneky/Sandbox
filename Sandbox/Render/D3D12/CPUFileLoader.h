#pragma once

#include "../FileLoader.h"
#include "Common/AsyncJobThread.h"

struct Device;
struct CommandList;
struct Resource;

struct LoadingJob
{
  Device& device;
  HANDLE fileHandle;
  int byteOffset;
  PixelFormat pixelFormat;
  eastl::unique_ptr< Resource >& uploadResource;
};

class CPUFileLoader : public FileLoader, public FileLoaderQueue, public AsyncJobThread< LoadingJob >
{
public:
  CPUFileLoader( Device& device );
  ~CPUFileLoader();

  eastl::unique_ptr< FileLoaderFile > OpenFile( const eastl::wstring& path ) override;

  void Enqueue( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat, eastl::unique_ptr< Resource >& uploadResource ) override;

private:
  struct CPUFile : public FileLoaderFile
  {
    CPUFile( FileLoaderQueue& loaderQueue, HANDLE fileHandle );
    ~CPUFile();

    void LoadPackedMipTail( Device& device
                          , CommandList& commandList
                          , Resource& resource
                          , const TFFHeader& tffHeader
                          , int blockSize ) override;

    void LoadSingleTile( Device& device
                       , CommandList& commandList
                       , TileHeap::Allocation allocation
                       , int byteOffset
                       , OnTileLoadAction onTileLoadAction ) override;

    void UploadLoadedTiles( Device& device, CommandQueue& copyQueue, CommandList& commandList ) override;

    struct LoadingJob
    {
      eastl::unique_ptr< Resource > uploadResource;
      Resource*                     targetResource;
      OnTileLoadAction              onTileLoadAction;
      int                           tileX;
      int                           tileY;
    };

    void UploadTile( Device& device
                   , CommandQueue& copyQueue
                   , CommandList& commandList
                   , LoadingJob&& loadingJob );

    FileLoaderQueue& loaderQueue;
    HANDLE fileHandle;

    eastl::list< LoadingJob > loadingTiles;
  };
};