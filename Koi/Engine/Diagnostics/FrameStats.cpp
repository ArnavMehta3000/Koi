#include "Koi/Engine/Diagnostics/FrameStats.h"
#include <chrono>
#include <utility>
#include <vector>

namespace koi
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		Clock::time_point            s_frameStart = Clock::now();
		std::vector<FrameStatSample> s_current;
		std::vector<FrameStatSample> s_last;
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
		std::swap(s_current, s_last);
		s_current.clear();
		s_frameStart = Clock::now();
	}

	std::span<const FrameStatSample> FrameStats::GetLastFrame()
	{
		return s_last;
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
