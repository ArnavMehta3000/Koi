#pragma once
#include "Koi/Engine/Core/Types.h"
#include <chrono>

namespace koi
{
	class Timer : public NonCopyable
	{
	public:
		Timer();

		void SetMaxDeltaTime(f64 seconds)     { m_maxDeltaTime = seconds; }

		[[nodiscard]] f32  GetDeltaTime()  const { return static_cast<f32>(m_elapsedTime); }
		[[nodiscard]] f64  GetTotalTime()  const { return m_totalTime; }
		[[nodiscard]] u64  GetFrameCount() const { return m_frameCount; }
		[[nodiscard]] u32  GetFPS()        const { return m_fps; }

		void Reset();
		void Tick();

	private:
		using Clock     = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		TimePoint m_lastTime{};

		f64 m_elapsedTime     = 0.0;
		f64 m_totalTime       = 0.0;
		f64 m_maxDeltaTime    = 0.1;

		u64 m_frameCount     = 0;
		u32 m_fps            = 0;
		u32 m_fpsCounter     = 0;
		f64 m_fpsAccumulator = 0.0;
	};
}
