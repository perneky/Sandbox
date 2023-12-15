#pragma once

struct TFFHeader
{
  enum class PixelFormat : uint32_t
  {
    Unsupported,
    BC1,
    BC2,
    BC3,
    BC4,
    BC5,
  };

  uint32_t width;
  uint32_t height;
  uint32_t tileWidth;
  uint32_t tileHeight;
  uint32_t mipCount;
  PixelFormat pixelFormat;

  uint32_t packedMipCount;
  uint32_t packedMipDataSize;

  int getBlockSize()
  {
    switch ( pixelFormat )
    {
    case TFFHeader::PixelFormat::BC1: return 8;
    case TFFHeader::PixelFormat::BC2: return 16;
    case TFFHeader::PixelFormat::BC3: return 16;
    default: assert( false ); return 0;
    }
  }

  int calcMipSize( int mip )
  {
    int mipBlockWidth  = ( int( width  ) / 4 ) >> mip;
    int mipBlockHeight = ( int( height ) / 4 ) >> mip;
    mipBlockWidth  = mipBlockWidth  < 1 ? 1 : mipBlockWidth;
    mipBlockHeight = mipBlockHeight < 1 ? 1 : mipBlockHeight;
    return mipBlockWidth * mipBlockHeight * getBlockSize();
  }
};
