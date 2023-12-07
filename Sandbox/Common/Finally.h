#pragma once

struct Finally
{
  using Func = eastl::function< void() >;

  Finally() = default;
  Finally( Func&& func ) : func( func ) {}
  ~Finally() { if ( func ) func(); }

  Finally( const Finally& ) = delete;
  Finally( const Finally&& other ) : func( eastl::move( other.func ) ) {}
  Finally& operator = ( const Finally& ) = delete;
  Finally& operator = ( const Finally&& other ) { func = eastl::move( other.func ); }

  Func func;
};