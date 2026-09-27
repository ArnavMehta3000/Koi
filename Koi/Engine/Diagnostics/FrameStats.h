#pragma once
#include "Koi/Engine/Core/Types.h"
#include <span>

namespace koi
{
	struct FrameStatSample
	{
		std::string_view Name;
		f64              Start;  // seconds since the frame began
		f64              End;
	};

	struct FrameCounter
	{
		std::string_view Name;
		i64              Value;
	};

	class FrameStats
	{
	public:
		static void NewFrame();
		[[nodiscard]] static std::span<const FrameStatSample> GetLastFrame();

		static void Count(std::string_view name, i64 value);
		[[nodiscard]] static std::span<const FrameCounter> GetLastCounters();
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
