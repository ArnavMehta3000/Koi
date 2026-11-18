#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Core/Types.h"
#include <atomic>
#include <cstdlib>
#include <new>

namespace koi::internal
{
	constinit std::atomic<u64> g_allocationCount{ 0 };
}

#if !KOI_RELEASE

namespace
{
	void* Alloc(std::size_t size, std::size_t align)
	{
		void* p = ::_aligned_malloc(size ? size : 1, align);
		if (p)
		{
			koi::internal::g_allocationCount.fetch_add(1, std::memory_order_relaxed);
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

	constexpr std::size_t kDefaultAlign = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
}

void* operator new(std::size_t s)                                                       { if (void* p = Alloc(s, kDefaultAlign)) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t s)                                                     { if (void* p = Alloc(s, kDefaultAlign)) return p; throw std::bad_alloc{}; }
void* operator new(std::size_t s, std::align_val_t a)                                   { if (void* p = Alloc(s, std::size_t(a))) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t s, std::align_val_t a)                                 { if (void* p = Alloc(s, std::size_t(a))) return p; throw std::bad_alloc{}; }
void* operator new(std::size_t s, const std::nothrow_t&) noexcept                       { return Alloc(s, kDefaultAlign); }
void* operator new[](std::size_t s, const std::nothrow_t&) noexcept                     { return Alloc(s, kDefaultAlign); }
void* operator new(std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept   { return Alloc(s, std::size_t(a)); }
void* operator new[](std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept { return Alloc(s, std::size_t(a)); }

void operator delete  (void* p)                                          noexcept { Free(p); }
void operator delete[](void* p)                                          noexcept { Free(p); }
void operator delete  (void* p, std::size_t)                             noexcept { Free(p); }
void operator delete[](void* p, std::size_t)                             noexcept { Free(p); }
void operator delete  (void* p, std::align_val_t)                        noexcept { Free(p); }
void operator delete[](void* p, std::align_val_t)                        noexcept { Free(p); }
void operator delete  (void* p, std::size_t, std::align_val_t)           noexcept { Free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t)           noexcept { Free(p); }
void operator delete  (void* p, const std::nothrow_t&)                   noexcept { Free(p); }
void operator delete[](void* p, const std::nothrow_t&)                   noexcept { Free(p); }
void operator delete  (void* p, std::align_val_t, const std::nothrow_t&) noexcept { Free(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { Free(p); }

#endif
