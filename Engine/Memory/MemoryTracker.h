#pragma once
#include "Memory/MemoryTags.h"
#include <array>
#include <atomic>

namespace koi
{
	struct TagStats
	{
		std::atomic<std::size_t> Current   { 0 };
		std::atomic<std::size_t> Peak      { 0 };
		std::atomic<std::size_t> AllocCount{ 0 };
		std::atomic<std::size_t> FreeCount { 0 };
	};

	inline constexpr std::size_t kTagStackDepth = 16;

	class MemoryTracker
	{
	public:
		static MemoryTracker& Get() noexcept
		{
			static MemoryTracker t;
			return t;
		}

		void RecordAlloc(MemTag tag, std::size_t bytes) noexcept
		{
			TagStats& stats = m_stats[static_cast<std::size_t>(tag)];
			auto current = stats.Current.fetch_add(bytes, std::memory_order_relaxed) + bytes;
			stats.AllocCount.fetch_add(1, std::memory_order_relaxed);
			auto peak = stats.Peak.load(std::memory_order_relaxed);

			while (current > peak
				&& !stats.Peak.compare_exchange_weak(peak, current, std::memory_order_relaxed))
			{
				// Empty
			}
		}

		void RecordFree(MemTag tag, std::size_t bytes) noexcept
		{
			TagStats& stats = m_stats[static_cast<std::size_t>(tag)];
			stats.Current.fetch_sub(bytes, std::memory_order_relaxed);
			stats.FreeCount.fetch_add(1, std::memory_order_relaxed);
		}

		const TagStats& GetStats(MemTag tag) const noexcept
		{
			return m_stats[static_cast<std::size_t>(tag)];
		}

	private:
		MemoryTracker() = default;

	private:
		std::array<TagStats, static_cast<std::size_t>(MemTag::MAX_COUNT)> m_stats;
	};

	struct TagStack
	{
		std::array<MemTag, kTagStackDepth> Data{};
		std::size_t Depth = 0;

		inline MemTag Current() const noexcept { return Depth == 0 ? MemTag::Untagged : Data[Depth - 1]; }
		inline void Push(MemTag tag) { if (Depth < kTagStackDepth) Data[Depth] = tag; ++Depth; }
		inline void Pop() noexcept { if (Depth > 0) --Depth; }
	};

	inline thread_local TagStack t_tagStack;

	class MemTagScope
	{
	public:
		explicit MemTagScope(MemTag t) noexcept { t_tagStack.Push(t); }
		~MemTagScope() noexcept { t_tagStack.Pop(); }
		MemTagScope(const MemTagScope&) = delete;
		MemTagScope& operator=(const MemTagScope&) = delete;
	};

	struct MemTagSnapshot
	{
		MemTag Tag;
		std::size_t Current;
		std::size_t Peak;
		std::size_t Allocs;
		std::size_t Frees;
	};
	using MemTagSnapshotArray = std::array<MemTagSnapshot, static_cast<std::size_t>(MemTag::MAX_COUNT)>;

	MemTagSnapshotArray GetMemSnapshot();


#define KOI_MEM_TAG_SCOPE(tag) ::koi::MemTagScope _koi_mem_tag_scope_##__LINE__{tag}
}
