#include "Memory/MemoryTracker.h"
#include "Core/Profiling.h"
#include <new>

namespace koi
{
	namespace
	{
		struct AllocHeader
		{
			std::size_t Size;
			MemTag Tag;
		};

		// Header sits right before the user pointer. Over-allocate to keep alignment.
		constexpr std::size_t kHeaderSize = sizeof(AllocHeader);
		constexpr std::size_t kMaxAlign   = alignof(std::max_align_t);
		constexpr std::size_t kHeaderPad  = (kHeaderSize + kMaxAlign - 1) & ~(kMaxAlign - 1);

		void* TrackedAlloc(std::size_t size, std::size_t align)
		{
			// We need space for: [raw_ptr][header][padding][user_data]
			//                                              ^ aligned to `align`
			constexpr std::size_t kPrefix = sizeof(void*) + sizeof(AllocHeader);

			// Over-allocate enough that we can find an aligned user address
			// with at least kPrefix bytes of room before it.
			std::size_t total = size + kPrefix + align - 1;

			void* raw = std::malloc(total);
			if (!raw)
				return nullptr;

			// Compute the user pointer: aligned, with kPrefix bytes available before it.
			auto rawAddr  = reinterpret_cast<std::uintptr_t>(raw);
			auto userAddr = (rawAddr + kPrefix + align - 1) & ~(align - 1);
			auto* user    = reinterpret_cast<std::byte*>(userAddr);

			// Lay out the metadata immediately before the user pointer.
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

	MemTagSnapshotArray koi::GetMemSnapshot()
	{
		std::array<MemTagSnapshot, static_cast<std::size_t>(MemTag::MAX_COUNT)> out{};

		auto& t = MemoryTracker::Get();

		for (std::size_t i = 0; i < out.size(); ++i)
		{
			auto tag      = static_cast<MemTag>(i);
			const auto& s = t.GetStats(tag);

			out[i] =
			{
				tag,
				s.Current.load(),
				s.Peak.load(),
				s.AllocCount.load(),
				s.FreeCount.load()
			};
		}
		return out;
	}
}

void* operator new(std::size_t size)
{
	if (void* p = koi::TrackedAlloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__)) return p;
	throw std::bad_alloc{};
}

void* operator new[](std::size_t size)
{
	if (void* p = koi::TrackedAlloc(size, __STDCPP_DEFAULT_NEW_ALIGNMENT__)) return p;
	throw std::bad_alloc{};
}

void* operator new(std::size_t size, std::align_val_t a)
{
	if (void* p = koi::TrackedAlloc(size, static_cast<std::size_t>(a))) return p;
	throw std::bad_alloc{};
}

void* operator new[](std::size_t size, std::align_val_t a)
{
	if (void* p = koi::TrackedAlloc(size, static_cast<std::size_t>(a))) return p;
	throw std::bad_alloc{};
}

void* operator new(std::size_t s, const std::nothrow_t&) noexcept
{
	return koi::TrackedAlloc(s, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

void* operator new[](std::size_t s, const std::nothrow_t&) noexcept
{
	return koi::TrackedAlloc(s, __STDCPP_DEFAULT_NEW_ALIGNMENT__);
}

void* operator new(std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept
{
	return koi::TrackedAlloc(s, static_cast<std::size_t>(a));
}

void* operator new[](std::size_t s, std::align_val_t a, const std::nothrow_t&) noexcept
{
	return koi::TrackedAlloc(s, static_cast<std::size_t>(a));
}

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
