#pragma once

inline eastl::vector< uint8_t > ReadFileToMemory( const wchar_t* filePath )
{
#ifdef _DEBUG
  eastl::wstring debugPath( filePath );
  if ( debugPath.find( L".cso" ) != eastl::wstring::npos )
  {
    debugPath.insert( debugPath.size() - 4, L"_d" );
    filePath = debugPath.data();
  }
#endif

  std::ifstream   file( filePath, std::ios::binary | std::ios::ate );
  std::streamsize size = file.tellg();
  file.seekg( 0, std::ios::beg );

  if ( size < 0 )
    return {};

  eastl::vector< uint8_t > buffer( size );
  if ( file.read( (char*)buffer.data(), size ) )
    return buffer;

  return {};
}

inline bool read( FILE* handle, void* data, int dataSize )
{
  if ( fread_s( data, dataSize, dataSize, 1, handle ) != 1 )
    return false;

  return true;
}

template< typename T >
inline bool read( FILE* handle, T& data )
{
  return read( handle, &data, sizeof( data ) );
}
