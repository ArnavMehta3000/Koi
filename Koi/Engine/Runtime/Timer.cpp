#include "Koi/Engine/Runtime/Timer.h"

namespace koi
{
	Timer::Timer()
		: m_lastTime(Clock::now())
	{
	}

	void Timer::Reset()
	{
		m_lastTime        = Clock::now();
		m_elapsedTime     = 0.0;
		m_totalTime       = 0.0;
		m_frameCount      = 0;
		m_fps             = 0;
		m_fpsCounter      = 0;
		m_fpsAccumulator  = 0.0;
	}

	void Timer::Tick()
	{
		const TimePoint now = Clock::now();
		f64 elapsed = std::chrono::duration<f64>(now - m_lastTime).count();
		m_lastTime  = now;

		// Capped
		if (elapsed > m_maxDeltaTime)
			elapsed = m_maxDeltaTime;

		m_elapsedTime = elapsed;
		m_totalTime  += elapsed;
		++m_frameCount;

		m_fpsAccumulator += elapsed;
		++m_fpsCounter;
		if (m_fpsAccumulator >= 1.0)
		{
			m_fps            = m_fpsCounter;
			m_fpsCounter     = 0;
			m_fpsAccumulator -= 1.0;
		}
	}
}
