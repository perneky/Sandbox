#pragma once

#include "../FileLoader.h"

struct Device;
struct CommandList;
struct Resource;

class CPUFileLoader : public FileLoader, public FileLoaderQueue
{
public:
  CPUFileLoader( Device& device );
  ~CPUFileLoader();

  eastl::unique_ptr< FileLoaderFile > OpenFile( const eastl::wstring& path ) override;

  void Enqueue( Device& device, HANDLE fileHandle, int byteOffset, PixelFormat pixelFormat, eastl::unique_ptr< Resource >& uploadResource ) override;

private:
  struct LoadingJob
  {
    Device& device;
    HANDLE fileHandle;
    int byteOffset;
    PixelFormat pixelFormat;
    eastl::unique_ptr< Resource >& uploadResource;
  };

  eastl::atomic< bool > keepLoading = true;
  eastl::queue< LoadingJob > loadingJobs;

  CRITICAL_SECTION queueLock;
  HANDLE hasWork = INVALID_HANDLE_VALUE;

  std::thread loadingThread;

  static void LoadingThreadFunc( CPUFileLoader& loader );

  struct CPUFile : public FileLoaderFile
  {
    CPUFile( FileLoaderQueue& loaderQueue, HANDLE fileHandle );
    ~CPUFile();

    void LoadPackedMipTail( Device& device
                          , CommandList& commandList
                          , Resource& resource
                          , int byteOffset
                          , int byteCount
                          , int mipCount
                          , int firstMipHBlocks
                          , int firstMipVBlocks
                          , int blockSize ) override;

    void LoadSingleTile( Device& device
                       , CommandList& commandList
                       , TileHeap::Allocation allocation
                       , int byteOffset
                       , OnTileLoadAction onTileLoadAction ) override;

    void UploadLoadedTiles( Device& device, CommandList& commandList ) override;

    struct LoadingJob
    {
      eastl::unique_ptr< Resource > uploadResource;
      Resource*                     targetResource;
      OnTileLoadAction              onTileLoadAction;
      int                           tileX;
      int                           tileY;
    };

    void UploadTile( Device& device
                   , CommandList& commandList
                   , LoadingJob&& loadingJob );

    FileLoaderQueue& loaderQueue;
    HANDLE fileHandle;

    eastl::list< LoadingJob > loadingTiles;
  };
};