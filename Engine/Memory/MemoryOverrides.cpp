#include "Core/Profiling.h"
#include <new>

import Koi.Memory.MemoryTracker;

namespace koi
{
	namespace
	{
		struct AllocHeader
		{
			std::size_t Size;
			MemTag      Tag;
		};

		constexpr std::size_t kHeaderSize = sizeof(AllocHeader);
		constexpr std::size_t kMaxAlign   = alignof(std::max_align_t);
		constexpr std::size_t kHeaderPad  = (kHeaderSize + kMaxAlign - 1) & ~(kMaxAlign - 1);

		void* TrackedAlloc(std::size_t size, std::size_t align)
		{
			constexpr std::size_t kPrefix = sizeof(void*) + sizeof(AllocHeader);
			std::size_t total = size + kPrefix + align - 1;

			void* raw = std::malloc(total);
			if (!raw) return nullptr;

			auto rawAddr  = reinterpret_cast<std::uintptr_t>(raw);
			auto userAddr = (rawAddr + kPrefix + align - 1) & ~(align - 1);
			auto* user    = reinterpret_cast<std::byte*>(userAddr);

			auto* header   = reinterpret_cast<AllocHeader*>(user - sizeof(AllocHeader));
			auto** rawSlot = reinterpret_cast<void**>(user - sizeof(AllocHeader) - sizeof(void*));

			MemTag tag   = t_tagStack.Current();
			header->Size = size;
			header->Tag  = tag;
			*rawSlot     = raw;

			TracySecureAllocN(user, size, ToString(tag).data());
			MemoryTracker::Get().RecordAlloc(tag, size);
			return user;
		}

		void TrackedFree(void* ptr) noexcept
		{
			if (!ptr) return;

			auto* user     = static_cast<std::byte*>(ptr);
			auto* hdr      = reinterpret_cast<AllocHeader*>(user - sizeof(AllocHeader));
			auto** rawSlot = reinterpret_cast<void**>(user - sizeof(AllocHeader) - sizeof(void*));

			TracySecureFreeN(ptr, ToString(hdr->Tag).data());
			MemoryTracker::Get().RecordFree(hdr->Tag, hdr->Size);
			std::free(*rawSlot);
		}
	}
}

void* operator new  (std::size_t size)                              { if (void* p = koi::TrackedAlloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__)) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t size)                              { if (void* p = koi::TrackedAlloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__)) return p; throw std::bad_alloc{}; }
void* operator new  (std::size_t size, std::align_val_t a)          { if (void* p = koi::TrackedAlloc(size, static_cast<std::size_t>(a))) return p; throw std::bad_alloc{}; }
void* operator new[](std::size_t size, std::align_val_t a)          { if (void* p = koi::TrackedAlloc(size, static_cast<std::size_t>(a))) return p; throw std::bad_alloc{}; }
void* operator new  (std::size_t s, const std::nothrow_t&) noexcept { return koi::TrackedAlloc(s, __STDCPP_DEFAULT_NEW_ALIGNMENT__); }
void* operator new[](std::size_t s, const std::nothrow_t&) noexcept { return koi::TrackedAlloc(s, __STDCPP_DEFAULT_NEW_ALIGNMENT__); }
void* operator new  (std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept { return koi::TrackedAlloc(s, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept { return koi::TrackedAlloc(s, static_cast<std::size_t>(a)); }

void operator delete  (void* p)                                          noexcept { koi::TrackedFree(p); }
void operator delete[](void* p)                                          noexcept { koi::TrackedFree(p); }
void operator delete  (void* p, std::size_t)                             noexcept { koi::TrackedFree(p); }
void operator delete[](void* p, std::size_t)                             noexcept { koi::TrackedFree(p); }
void operator delete  (void* p, std::align_val_t)                        noexcept { koi::TrackedFree(p); }
void operator delete[](void* p, std::align_val_t)                        noexcept { koi::TrackedFree(p); }
void operator delete  (void* p, std::size_t, std::align_val_t)           noexcept { koi::TrackedFree(p); }
void operator delete[](void* p, std::size_t, std::align_val_t)           noexcept { koi::TrackedFree(p); }
void operator delete  (void* p, const std::nothrow_t&)                   noexcept { koi::TrackedFree(p); }
void operator delete[](void* p, const std::nothrow_t&)                   noexcept { koi::TrackedFree(p); }
void operator delete  (void* p, std::align_val_t, const std::nothrow_t&) noexcept { koi::TrackedFree(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { koi::TrackedFree(p); }
