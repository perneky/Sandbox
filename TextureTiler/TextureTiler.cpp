#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cassert>
#include <intrin.h>

#include "../Sandbox/Render/TextureStreamers/TFFFormat.h"

#define MAKEFOURCC(ch0, ch1, ch2, ch3) ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) | ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))

struct DDS_HEADER
{
  struct DDS_PIXELFORMAT
  {
    uint32_t    size;
    uint32_t    flags;
    uint32_t    fourCC;
    uint32_t    RGBBitCount;
    uint32_t    RBitMask;
    uint32_t    GBitMask;
    uint32_t    BBitMask;
    uint32_t    ABitMask;
  };

  uint32_t        size;
  uint32_t        flags;
  uint32_t        height;
  uint32_t        width;
  uint32_t        pitchOrLinearSize;
  uint32_t        depth;
  uint32_t        mipMapCount;
  uint32_t        reserved1[ 11 ];
  DDS_PIXELFORMAT ddspf;
  uint32_t        caps;
  uint32_t        caps2;
  uint32_t        caps3;
  uint32_t        caps4;
  uint32_t        reserved2;
};

static constexpr uint32_t DDS_MAGIC  = 0x20534444;
static constexpr uint32_t DDS_FOURCC = 0x00000004;

static bool isPowerOfTwo( uint32_t n )
{
  return n > 0 && !( n & ( n - 1 ) );
}

static TFFHeader::PixelFormat getPixelFormat( const DDS_HEADER& dds )
{
  switch ( dds.ddspf.fourCC )
  {
  case MAKEFOURCC( 'D', 'X', 'T', '1' ): return TFFHeader::PixelFormat::BC1;
  case MAKEFOURCC( 'D', 'X', 'T', '2' ):
  case MAKEFOURCC( 'D', 'X', 'T', '3' ): return TFFHeader::PixelFormat::BC2;
  case MAKEFOURCC( 'D', 'X', 'T', '4' ):
  case MAKEFOURCC( 'D', 'X', 'T', '5' ): return TFFHeader::PixelFormat::BC3;
  case MAKEFOURCC( 'A', 'T', 'I', '1' ):
  case MAKEFOURCC( 'B', 'C', '4', 'U' ): return TFFHeader::PixelFormat::BC4;
  case MAKEFOURCC( 'B', 'C', '5', 'U' ):
  case MAKEFOURCC( 'A', 'T', 'I', '2' ): return TFFHeader::PixelFormat::BC5;
  default: return TFFHeader::PixelFormat::Unsupported;
  }
}

static int getBlockSize( TFFHeader::PixelFormat pixelFormat )
{
  switch ( pixelFormat )
  {
  case TFFHeader::PixelFormat::BC1: return 8;
  case TFFHeader::PixelFormat::BC2: return 16;
  case TFFHeader::PixelFormat::BC3: return 16;
  case TFFHeader::PixelFormat::BC4: return 8;
  case TFFHeader::PixelFormat::BC5: return 16;
  default: assert( false ); return 0;
  }
}

static int calcTileWidth( TFFHeader::PixelFormat pixelFormat )
{
  switch ( pixelFormat )
  {
  case TFFHeader::PixelFormat::BC1: return 512;
  case TFFHeader::PixelFormat::BC2: return 256;
  case TFFHeader::PixelFormat::BC3: return 256;
  case TFFHeader::PixelFormat::BC4: return 512;
  case TFFHeader::PixelFormat::BC5: return 256;
  default: assert( false ); return 0;
  }
}

static int calcTileHeight( TFFHeader::PixelFormat pixelFormat )
{
  switch ( pixelFormat )
  {
  case TFFHeader::PixelFormat::BC1: return 256;
  case TFFHeader::PixelFormat::BC2: return 256;
  case TFFHeader::PixelFormat::BC3: return 256;
  case TFFHeader::PixelFormat::BC4: return 256;
  case TFFHeader::PixelFormat::BC5: return 256;
  default: assert( false ); return 0;
  }
}

static int getBlockSize( const DDS_HEADER& dds )
{
  return getBlockSize( getPixelFormat( dds ) );
}

static int calcMipSize( const DDS_HEADER& dds, int mip )
{
  int mipBlockWidth  = std::max( ( int( dds.width  ) / 4 ) >> mip, 1 );
  int mipBlockHeight = std::max( ( int( dds.height ) / 4 ) >> mip, 1 );
  return mipBlockWidth * mipBlockHeight * getBlockSize( dds );
}

static bool read( FILE* handle, void* data, int dataSize )
{
  if ( fread_s( data, dataSize, dataSize, 1, handle ) != 1 )
  {
    std::cout << "File read error!\n";
    return false;
  }

  return true;
}

template< typename T >
static bool read( FILE* handle, T& data )
{
  return read( handle, &data, sizeof( data ) );
}

static bool write( FILE* handle, const void* data, int dataSize )
{
  if ( fwrite( data, dataSize, 1, handle ) != 1 )
  {
    std::cout << "File write error!\n";
    return false;
  }

  return true;
}

template< typename T >
static bool write( FILE* handle, const T& data )
{
  return write( handle, &data, sizeof( data ) );
}

static void WriteTile( const std::string& basePath, int mip, int tx, int ty, int width, int height, const void* tileData, int tileMemorySize, const DDS_HEADER& originalDDS )
{
  auto path = basePath + "_" + std::to_string( mip ) + "_" + std::to_string( tx ) + "_" + std::to_string( ty ) + ".dds";

  DDS_HEADER dds = originalDDS;

  dds.height            = height;
  dds.width             = width;
  dds.pitchOrLinearSize = ( dds.width / 4 ) * getBlockSize( originalDDS );
  dds.depth             = 1;
  dds.mipMapCount       = 1;

  FILE* outputTextureFileHandle = nullptr;
  if ( fopen_s( &outputTextureFileHandle, path.data(), "wb" ) )
    return;

  write( outputTextureFileHandle, DDS_MAGIC );
  write( outputTextureFileHandle, dds );
  write( outputTextureFileHandle, tileData, tileMemorySize );

  fclose( outputTextureFileHandle );
}

int main(int argc, char* argv[])
{
  namespace fs = std::filesystem;

  if ( argc < 2 )
  {
    std::cout << "Specify texture path!\n";
    return -1;
  }

  fs::path path( argv[ 1 ] );
  if ( path.extension() != ".dds" )
  {
    std::cout << "Texture needs to be DDS!\n";
    return -1;
  }

  FILE* inputTextureFileHandle = nullptr;
  if ( _wfopen_s( &inputTextureFileHandle, path.c_str(), L"rb" ) )
  {
    std::cout << "Failed to open file!\n";
    return -1;
  }

  uint32_t magic;
  if ( !read( inputTextureFileHandle, magic ) )
    return -1;

  if ( magic != DDS_MAGIC )
  {
    std::cout << "DDS magic mismatch!\n";
    return -1;
  }

  DDS_HEADER ddsHeader;
  if ( !read( inputTextureFileHandle, ddsHeader ) )
    return -1;

  if ( ddsHeader.size != sizeof( DDS_HEADER ) || ddsHeader.ddspf.size != sizeof( DDS_HEADER::DDS_PIXELFORMAT ) )
  {
    std::cout << "DDS header corrupted!\n";
    return -1;
  }

  if ( !isPowerOfTwo( ddsHeader.width ) || !isPowerOfTwo( ddsHeader.height ) )
  {
    std::cout << "Only POT textures are supported!\n";
    return -1;
  }

  if ( ddsHeader.depth != 1 )
  {
    std::cout << "Only 2d textures are supported!\n";
    return -1;
  }

  uint32_t maxSize = std::max( ddsHeader.width, ddsHeader.height );
  uint32_t tzcnt   = _tzcnt_u32( maxSize );
  if ( ddsHeader.mipMapCount != tzcnt + 1 )
  {
    std::cout << "Only textures with a full mipchain are supported!\n";
    return -1;
  }

  if ( ( ddsHeader.flags & DDS_FOURCC ) == 0 )
  {
    std::cout << "Only files with FCC flag are supported!\n";
    return -1;
  }

  if ( getPixelFormat( ddsHeader ) == TFFHeader::PixelFormat::Unsupported )
  {
    std::cout << "Only files with BC1, BC2, BC3, BC4 or BC5 format are supported!\n";
    return -1;
  }

  TFFHeader tffHeader;
  tffHeader.width          = ddsHeader.width;
  tffHeader.height         = ddsHeader.height;
  tffHeader.mipCount       = ddsHeader.mipMapCount;
  tffHeader.pixelFormat    = getPixelFormat( ddsHeader );
  tffHeader.tileWidth      = calcTileWidth( tffHeader.pixelFormat );
  tffHeader.tileHeight     = calcTileHeight( tffHeader.pixelFormat );

  int tailMipCount = 0;
  for ( int mip = 0; mip < int( ddsHeader.mipMapCount ); ++mip )
  {
    int mipWidth  = std::max( ddsHeader.width  >> mip, 1U );
    int mipHeight = std::max( ddsHeader.height >> mip, 1U );

    if ( mipWidth < int( tffHeader.tileWidth ) || mipHeight < int( tffHeader.tileHeight ) )
    {
      tailMipCount = ddsHeader.mipMapCount - mip;
      break;
    }
  }

  int tailMemSize = 0;
  for ( int mip = ddsHeader.mipMapCount - tailMipCount; mip < int( ddsHeader.mipMapCount ); ++mip )
    tailMemSize += calcMipSize( ddsHeader, mip );

  tffHeader.packedMipCount    = tailMipCount;
  tffHeader.packedMipDataSize = tailMemSize;

  auto outputFileName = path.generic_string();
  outputFileName.replace( outputFileName.size() - 3, 3, "tff" );
  FILE* outputTextureFileHandle = nullptr;
  if ( fopen_s( &outputTextureFileHandle, outputFileName.data(), "wb" ) )
  {
    std::cout << "Failed to open file!\n";
    return -1;
  }

  int tileBlockWidth  = int( tffHeader.tileWidth  / 4 );
  int tileBlockHeight = int( tffHeader.tileHeight / 4 );
  int tileMemorySize  = tileBlockWidth * tileBlockHeight * getBlockSize( ddsHeader );

  if ( !write( outputTextureFileHandle, tffHeader ) )
    return -1;

  int firstMip = ftell( inputTextureFileHandle );

  for ( int mip = 0; mip < int( ddsHeader.mipMapCount ) - tailMipCount; ++mip )
    fseek( inputTextureFileHandle, calcMipSize( ddsHeader, mip ), SEEK_CUR );

  uint8_t tileData[ 125 * 1024 ]; // Enough storage to store the tile with the border
  if ( !read( inputTextureFileHandle, tileData, tailMemSize ) )
    return -1;

  if ( !write( outputTextureFileHandle, tileData, tailMemSize ) )
    return -1;

  uint8_t* mipData = new uint8_t[ 1024 * 1024 * 1024 ]; // Just big enough to hold "any" size

  int sourceTileMemoryWidth = tileBlockWidth * getBlockSize( ddsHeader );

  fseek( inputTextureFileHandle, firstMip, SEEK_SET );
  for ( int mip = 0; mip < int( ddsHeader.mipMapCount ) - tailMipCount; ++mip )
  {
    int mipSize = calcMipSize( ddsHeader, mip );

    if ( !read( inputTextureFileHandle, mipData, mipSize ) )
      return -1;

    int mipBlockWidth  = std::max( ( int( ddsHeader.width  ) / 4 ) >> mip, 1 );
    int mipBlockHeight = std::max( ( int( ddsHeader.height ) / 4 ) >> mip, 1 );
    int htiles         = std::max( mipBlockWidth  / tileBlockWidth,  1 );
    int vtiles         = std::max( mipBlockHeight / tileBlockHeight, 1 );

    auto blockSize = getBlockSize( ddsHeader );

    for ( int ty = 0; ty < vtiles; ++ty )
    {
      for ( int tx = 0; tx < htiles; ++tx )
      {
        int lineStart   = ty * htiles * tileMemorySize;
        int readCursor  = lineStart + tx * sourceTileMemoryWidth;
        int writeCursor = 0;

        auto writeOneLine = [&]()
        {
          bool isLeftMostBlock  = tx == 0;
          bool isRightMostBlock = tx == htiles - 1;

          memcpy_s( tileData + writeCursor, sizeof( tileData ) - writeCursor, mipData + readCursor, sourceTileMemoryWidth );
          readCursor += mipBlockWidth * blockSize;
          writeCursor += sourceTileMemoryWidth;
        };

        bool isTopMostBlock    = ty == 0;
        bool isBottomMostBlock = ty == vtiles - 1;

        for ( int by = 0; by < tileBlockHeight; ++by )
          writeOneLine();

        if ( !write( outputTextureFileHandle, tileData, tileMemorySize ) )
          return -1;

        #if _DEBUG
          if ( forReserved )
            WriteTile( outputFileName, mip, tx, ty, tffHeader.tileWidth, tffHeader.tileHeight, tileData, tileMemorySize, ddsHeader );
          else
            WriteTile( outputFileName, mip, tx, ty, tffHeader.tileWidth + 8, tffHeader.tileHeight + 8, tileData, tileMemorySize, ddsHeader );
        #endif
      }
    }
  }

  fclose( outputTextureFileHandle );
  fclose( inputTextureFileHandle );

  return 0;
}
