#include "Koi/Engine/Diagnostics/FrameStats.h"
#include <atomic>
#include <chrono>
#include <utility>
#include <vector>

namespace koi
{
	namespace internal
	{
		extern std::atomic<u64> g_allocationCount;  // MemoryOverrides.cpp
	}

	namespace
	{
		using Clock = std::chrono::steady_clock;

		Clock::time_point            s_frameStart = Clock::now();
		std::vector<FrameStatSample> s_current;
		std::vector<FrameStatSample> s_last;
		std::vector<FrameCounter>    s_currentCounters;
		std::vector<FrameCounter>    s_lastCounters;
		u64                          s_allocationBase = ~0ull;
		u32                          s_depth = 0;
		thread_local bool            t_isMainThread = false;

		f64 Now()
		{
			return std::chrono::duration<f64>(Clock::now() - s_frameStart).count();
		}
	}

	void FrameStats::NewFrame()
	{
		t_isMainThread = true;

		const u64 allocations = internal::g_allocationCount.load(std::memory_order_relaxed);
		if (s_allocationBase != ~0ull)
			Count("Allocations", i64(allocations - s_allocationBase));
		s_allocationBase = allocations;

		std::swap(s_current, s_last);
		std::swap(s_currentCounters, s_lastCounters);
		s_current.clear();
		s_currentCounters.clear();
		s_frameStart = Clock::now();
	}

	std::span<const FrameStatSample> FrameStats::GetLastFrame()
	{
		return s_last;
	}

	void FrameStats::Count(std::string_view name, i64 value)
	{
		if (!t_isMainThread) return;

		for (FrameCounter& counter : s_currentCounters)
		{
			if (counter.Name == name)
			{
				counter.Value += value;
				return;
			}
		}
		s_currentCounters.push_back({ name, value });
	}

	std::span<const FrameCounter> FrameStats::GetLastCounters()
	{
		return s_lastCounters;
	}

	FrameStatScope::FrameStatScope(std::string_view name)
		: m_name(name)
		, m_counted(t_isMainThread)
	{
		if (!m_counted) return;

		m_active = s_depth++ == 0;
		if (m_active)
			m_start = Now();
	}

	FrameStatScope::~FrameStatScope()
	{
		if (!m_counted) return;

		--s_depth;
		if (m_active)
			s_current.push_back({ m_name, m_start, Now() });
	}
}
