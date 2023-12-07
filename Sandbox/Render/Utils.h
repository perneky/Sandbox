#pragma once

#include "Render/Resource.h"
#include "Render/Device.h"
#include "Render/CommandList.h"
#include "Render/Types.h"
#include "Render/RenderManager.h"

template< typename T >
inline eastl::unique_ptr< Resource > CreateBufferFromData( const T* firstElement
                                                       , int elementCount
                                                       , ResourceType resourceType
                                                       , Device& device
                                                       , CommandList& commandList
                                                       , const wchar_t* debugName )
{
  int  es = sizeof( T );
  int  bs = elementCount * es;

  auto resultBuffer = device.CreateBuffer( resourceType, HeapType::Default, false, bs, es, debugName );
  auto uploadBuffer = RenderManager::GetInstance().GetUploadBufferForResource( *resultBuffer );
  commandList.UploadBufferResource( eastl::move( uploadBuffer ), *resultBuffer, firstElement, es * elementCount );
  return resultBuffer;
}

inline eastl::pair< const void*, int > ParseSimpleDDS( const eastl::vector< uint8_t >& data, int& width, int& height, PixelFormat& pf )
{
  const uint32_t* cursor = reinterpret_cast< const uint32_t* >( data.data() );

  height = int( cursor[ 3 ] );
  width  = int( cursor[ 4 ] );

  #define ISBITMASK( r,g,b,a ) ( cursor[ 23 ] == r && cursor[ 24 ] == g && cursor[ 25 ] == b && cursor[ 26 ] == a )

  if ( cursor[ 20 ] & 0x00000040 )
  {
    if ( cursor[ 22 ] == 32 )
    {
      if ( ISBITMASK( 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000 ) )
        pf = PixelFormat::RGBA8888UN;
      else
        assert( false );
    }
    else
      assert( false );
  }
  else
    assert( false );

  #undef ISBITMASK

  static constexpr int headerSize = 32 * sizeof( uint32_t );
  return { data.data() + headerSize, int( data.size() ) - headerSize };
}