#include "Koi/Engine/Memory/Allocator.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include <atomic>
#include <cstdlib>

namespace koi::internal
{
	namespace
	{
		constinit std::atomic<u64> g_allocationCount{ 0 };
	}

	void* Allocate(std::size_t size, std::size_t align) noexcept
	{
		void* p = ::_aligned_malloc(size ? size : 1, align);
		if (p)
		{
			g_allocationCount.fetch_add(1, std::memory_order_relaxed);
			TracyAlloc(p, size);
		}
		return p;
	}

	void Free(void* p) noexcept
	{
		if (!p) return;
		TracyFree(p);
		::_aligned_free(p);
	}

	u64 GetAllocationCount() noexcept
	{
		return g_allocationCount.load(std::memory_order_relaxed);
	}
}
