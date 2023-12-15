#include "TextureStreamer_Tiled.h"
#include "Common/Files.h"
#include "Render/Device.h"
#include "Render/TileHeap.h"
#include "Render/RenderManager.h"
#include "Render/Resource.h"
#include "Render/CommandList.h"
#include "Render/ShaderValues.h"
#include "../FileLoader.h"

static void tff( eastl::wstring& path )
{
  path.replace( path.size() - 3, 3, L"tff" );
}

struct ProcessThread
{
  void Start( eastl::string&& name )
  {
    this->name = eastl::forward< eastl::string >( name );
    hasWork = CreateEvent( nullptr, false, false, nullptr );
    isIdle  = CreateEvent( nullptr, true,  false, nullptr );
    thread = std::thread( &ProcessThread::Work, this );
  }

  void Stop()
  {
    quit = true;
    SetEvent( hasWork );
    thread.join();
  }

  void Process( Device& device
              , CommandQueue& graphicsQueue
              , CommandQueue& copyQueue
              , CommandList& commandList
              , uint64_t fence
              , uint64_t frameNo
              , eastl::unique_ptr< Resource >* textures
              , uint32_t* globalFeedback
              , int begin
              , int end )
  {
    this->device            = &device;
    this->graphicsQueue     = &graphicsQueue;
    this->copyQueue         = &copyQueue;
    this->commandList       = &commandList;
    this->fence             = fence;
    this->frameNo           = frameNo;
    this->textures          = textures;
    this->globalFeedback    = globalFeedback;
    this->begin             = begin;
    this->end               = end;

    ResetEvent( isIdle );
    SetEvent( hasWork );
  }

  void Wait()
  {
    WaitForSingleObject( isIdle, INFINITE );
  }

  void Work()
  {
    SetThreadName( GetCurrentThreadId(), name.data() );

    while ( !quit )
    {
      // When starting the thread, the commandList is null, so we enter waiting.
      if ( commandList )
      {
        GPUSection gpuSection( *commandList, L"TiledTextureStreamer::UpdateAfterFrame" );

        for ( auto iter = begin; iter != end; ++iter )
          textures[ iter ]->EndFeedback( *graphicsQueue, *copyQueue, *device, *commandList, fence, frameNo, globalFeedback[ iter ] );
      }

      SetEvent( isIdle );
      WaitForSingleObject( hasWork, INFINITE );
    }
  }

  eastl::string name;

  std::thread thread;

  volatile bool quit = false;
  HANDLE isIdle  = INVALID_HANDLE_VALUE;
  HANDLE hasWork = INVALID_HANDLE_VALUE;

  Device*       device        = nullptr;
  CommandQueue* graphicsQueue = nullptr;
  CommandQueue* copyQueue     = nullptr;
  CommandList*  commandList   = nullptr;
  uint64_t      fence         = 0;
  uint64_t      frameNo       = 0;

  eastl::unique_ptr< Resource >* textures;
  uint32_t* globalFeedback;
  int begin;
  int end;
} workers[ 4 ];

TiledTextureStreamer::TiledTextureStreamer( Device& device )
{
  fileLoader = CreateFileLoader( device );

  assert( _countof( workers ) == 4 );

  workers[ 0 ].Start( "TextureStreamerWorker_1" );
  workers[ 1 ].Start( "TextureStreamerWorker_2" );
  workers[ 2 ].Start( "TextureStreamerWorker_3" );
  workers[ 3 ].Start( "TextureStreamerWorker_4" );

//  for ( int heapIx = 0; heapIx < 32; ++heapIx )
//    memoryHeaps.emplace_back( device.CreateMemoryHeap( 32 * 1024 * 1024, L"TiledTextureStreamer heap" ) );
}

TiledTextureStreamer::~TiledTextureStreamer()
{
  for ( auto& worker : workers )
    worker.Stop();
}

void TiledTextureStreamer::CacheTexture( CommandQueue& directQueue, CommandList& commandList, const eastl::wstring& path )
{
  auto tffPath = path;
  tff( tffPath );

  if ( textureMap.count( tffPath ) > 0 )
    return;

  auto& device = RenderManager::GetInstance().GetDevice();

  int slot = int( textures.size() );
  assert( slot < Scene2DResourceCount );

  TFFHeader tffHeader;

  {
    FILE* fileHandle = nullptr;
    if ( _wfopen_s( &fileHandle, tffPath.data(), L"rb" ) )
      return;

    auto headerRead = fread_s( &tffHeader, sizeof( tffHeader ), sizeof( tffHeader ), 1, fileHandle );
    fclose( fileHandle );

    if ( !headerRead )
      return;
  }

  auto fileHandle = fileLoader->OpenFile( tffPath.data() );
  auto texture = device.Stream2DTexture( directQueue
                                       , commandList
                                       , tffHeader
                                       , eastl::move( fileHandle )
                                       , Scene2DResourceBaseSlot + slot
                                       , tffPath.data() );

  textures.emplace_back( eastl::move( texture ) );
  textureMap[ tffPath ] = slot;
}

int TiledTextureStreamer::GetTextureSlot( const eastl::wstring& path, int* refTexutreId )
{
  auto tffPath = path;
  tffPath.replace( tffPath.size() - 3, 3, L"tff" );
  auto iter = textureMap.find( tffPath );
  if ( refTexutreId )
  {
    if ( iter == textureMap.end() )
      *refTexutreId = -1;
    else
    {
      auto mips = textures[ iter->second ]->GetTextureMipLevels();
      *refTexutreId = eastl::max( mips - 3, 0 );
    }
  }
  return iter == textureMap.end() ? -1 : iter->second;
}

Resource* TiledTextureStreamer::GetTexture( int index )
{
  return textures[ index ].get();
}

void TiledTextureStreamer::UpdateBeforeFrame( Device& device, CommandQueue& copyQueue, CommandList& commandList )
{
  for ( auto& texture : textures )
    texture->UploadLoadedTiles( device, copyQueue, commandList );
}

TextureStreamer::UpdateResult TiledTextureStreamer::UpdateAfterFrame( Device& device, CommandQueue& graphicsQueue, CommandQueue& copyQueue, CommandList& syncCommandList, uint64_t fence, uint32_t* globalFeedback )
{
  #if 0
    auto& renderManager = RenderManager::GetInstance();

    UpdateResult commandLists( _countof( workers ) );

    auto texturePerThread = int( textures.size() / _countof( workers ) ) + 1;

    int begin = 0;
    for ( int worker = 0; worker < _countof( workers ); ++worker )
    {
      auto end = eastl::min( begin + texturePerThread, int( textures.size() ) );
      if ( begin == end )
        break;

      commandLists[ worker ].second = renderManager.RequestCommandAllocator( CommandQueueType::Direct );
      commandLists[ worker ].first  = renderManager.CreateCommandList( *commandLists[ worker ].second, CommandQueueType::Direct );

      workers[ worker ].Process( graphicsQueue, copyQueue, device, *commandLists[ worker ].first, fence, frameNo, textures.data(), globalFeedback, begin, end );
      begin = end;
    }

    for ( auto& worker : workers )
      worker.Wait();

    while ( !commandLists.back().first )
      commandLists.pop_back();

    return commandLists;
  #else
    // Used to run without threads
    int index = 0;
    for ( auto& texture : textures )
      texture->EndFeedback( graphicsQueue, copyQueue, device, syncCommandList, fence, frameNo, globalFeedback[ index++ ] );

    ++frameNo;

    return {};
  #endif
}

int TiledTextureStreamer::Get2DTextureCount() const
{
  return int( textures.size() );
}

TiledTextureStreamer::MemoryStats TiledTextureStreamer::GetMemoryStats() const
{
  MemoryStats stats = {};

  stats.textureCount = int( textures.size() );

  for ( auto& texture : textures )
  {
    stats.virtualAllocationSize  += texture->GetVirtualAllocationSize ();
    stats.physicalAllocationSize += texture->GetPhysicalAllocationSize();
  }

  return stats;
}
