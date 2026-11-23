#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Memory/Allocator.h"
#include <new>

// Global new/delete, counted and reported to Tracy outside game-release
#if !KOI_RELEASE

namespace
{
	using koi::internal::Allocate;
	using koi::internal::Free;

	constexpr std::size_t kDefaultAlign = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
}

void* operator new(std::size_t s)                                                       { if (void* p = Allocate(s, kDefaultAlign)) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t s)                                                     { if (void* p = Allocate(s, kDefaultAlign)) return p; throw std::bad_alloc{}; }
void* operator new(std::size_t s, std::align_val_t a)                                   { if (void* p = Allocate(s, std::size_t(a))) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t s, std::align_val_t a)                                 { if (void* p = Allocate(s, std::size_t(a))) return p; throw std::bad_alloc{}; }
void* operator new(std::size_t s, const std::nothrow_t&) noexcept                       { return Allocate(s, kDefaultAlign); }
void* operator new[](std::size_t s, const std::nothrow_t&) noexcept                     { return Allocate(s, kDefaultAlign); }
void* operator new(std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept   { return Allocate(s, std::size_t(a)); }
void* operator new[](std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept { return Allocate(s, std::size_t(a)); }

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
