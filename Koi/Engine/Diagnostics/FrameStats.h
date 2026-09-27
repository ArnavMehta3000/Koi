#pragma once
#include "Koi/Engine/Core/Types.h"
#include <span>

namespace koi
{
	struct FrameStatSample
	{
		std::string_view Name;   // must outlive the frame, KOI_SCOPE passes literals
		f64              Start;  // seconds since the frame began
		f64              End;
	};

	// Flat per-frame CPU timings for the editor profiler. Main thread only, nested scopes are ignored
	class FrameStats
	{
	public:
		static void NewFrame();
		[[nodiscard]] static std::span<const FrameStatSample> GetLastFrame();
	};

	class FrameStatScope : public NonCopyable
	{
	public:
		explicit FrameStatScope(std::string_view name);
		~FrameStatScope();

	private:
		std::string_view m_name;
		f64              m_start   = 0.0;
		bool             m_counted = false;
		bool             m_active  = false;
	};
}
