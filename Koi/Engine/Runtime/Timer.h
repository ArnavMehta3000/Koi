#pragma once
#include "Koi/Engine/Core/Types.h"
#include <chrono>

namespace koi
{
	class Timer : public NonCopyable
	{
	public:
		Timer();

		void SetFixedTimestep(bool fixed)     { m_fixedTimestep   = fixed; }
		void SetTargetFPS(u32 fps)            { m_targetDeltaTime = 1.0 / static_cast<f64>(fps); }
		void SetTargetDeltaTime(f64 seconds)  { m_targetDeltaTime = seconds; }
		void SetMaxDeltaTime(f64 seconds)     { m_maxDeltaTime    = seconds; }

		[[nodiscard]] bool IsFixedTimestep()  const { return m_fixedTimestep; }
		[[nodiscard]] f32  GetDeltaTime()     const { return static_cast<f32>(m_elapsedTime); }
		[[nodiscard]] f64  GetTotalTime()     const { return m_totalTime; }
		[[nodiscard]] u64  GetFrameCount()    const { return m_frameCount; }
		[[nodiscard]] u32  GetFPS()           const { return m_fps; }

		void Reset();

		template<typename Fn>
		void Tick(Fn&& fn)
		{
			const TimePoint now = Clock::now();
			f64 elapsed = std::chrono::duration<f64>(now - m_lastTime).count();
			m_lastTime  = now;

			// Cap delta to prevent a spiral of death after breakpoints or heavy stalls.
			if (elapsed > m_maxDeltaTime)
				elapsed = m_maxDeltaTime;

			m_elapsedTime = elapsed;
			++m_frameCount;

			if (m_fixedTimestep)
			{
				m_accumulatedTime += elapsed;

				while (m_accumulatedTime >= m_targetDeltaTime)
				{
					m_accumulatedTime -= m_targetDeltaTime;
					m_totalTime       += m_targetDeltaTime;
					fn(static_cast<f32>(m_targetDeltaTime));
				}
			}
			else
			{
				m_totalTime += elapsed;
				fn(static_cast<f32>(elapsed));
			}

			// Update FPS counter once per second.
			m_fpsAccumulator += elapsed;
			++m_fpsCounter;
			if (m_fpsAccumulator >= 1.0)
			{
				m_fps            = m_fpsCounter;
				m_fpsCounter     = 0;
				m_fpsAccumulator -= 1.0;
			}
		}

	private:
		using Clock     = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		TimePoint m_lastTime{};

		f64 m_elapsedTime     = 0.0;
		f64 m_totalTime       = 0.0;
		f64 m_accumulatedTime = 0.0;
		f64 m_targetDeltaTime = 1.0 / 60.0;
		f64 m_maxDeltaTime    = 0.1;

		u64 m_frameCount     = 0;
		u32 m_fps            = 0;
		u32 m_fpsCounter     = 0;
		f64 m_fpsAccumulator = 0.0;

		bool m_fixedTimestep = false;
	};
}
