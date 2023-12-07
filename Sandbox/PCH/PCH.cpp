#include <EASTL-3.21.12/source/allocator_eastl.cpp>
#include <EASTL-3.21.12/source/assert.cpp>
#include <EASTL-3.21.12/source/atomic.cpp>
#include <EASTL-3.21.12/source/fixed_pool.cpp>
#include <EASTL-3.21.12/source/hashtable.cpp>
#include <EASTL-3.21.12/source/intrusive_list.cpp>
#include <EASTL-3.21.12/source/numeric_limits.cpp>
#include <EASTL-3.21.12/source/red_black_tree.cpp>
#include <EASTL-3.21.12/source/string.cpp>
#include <EASTL-3.21.12/source/thread_support.cpp>
#include <EAStdC-1.26.03/source/EASprintf.cpp>
#include <EAStdC-1.26.03/source/EASprintfCore.cpp>
#include <EAStdC-1.26.03/source/EAString.cpp>
#include <EAStdC-1.26.03/source/EAStdC.cpp>
#include <EAStdC-1.26.03/source/EAMemory.cpp>
#include <EAStdC-1.26.03/source/EACType.cpp>

namespace eastl
{
	allocator::allocator( const char* EASTL_NAME( pName ) )
	{
	}


	allocator::allocator( const allocator& EASTL_NAME( alloc ) )
	{
	}


	allocator::allocator( const allocator&, const char* EASTL_NAME( pName ) )
	{
	}

	allocator& allocator::operator=( const allocator& EASTL_NAME( alloc ) )
	{
		return *this;
	}

	const char* allocator::get_name() const
	{
		return EASTL_ALLOCATOR_DEFAULT_NAME;
	}

	void allocator::set_name( const char* EASTL_NAME( pName ) )
	{
	}

	void* allocator::allocate( size_t n, int flags )
	{
		return _aligned_offset_malloc( n, 1, 0 );
	}

	void* allocator::allocate( size_t n, size_t alignment, size_t offset, int flags )
	{
		return _aligned_offset_malloc( n, alignment, offset );
	}

	void allocator::deallocate( void* p, size_t )
	{
		_aligned_free( p );
	}

	bool operator==( const allocator&, const allocator& )
	{
		return true; // All allocators are considered equal, as they merely use global new/delete.
	}

	allocator* GetDefaultAllocator()
	{
		static allocator allocator;
		return &allocator;
	}
} // namespace eastl
