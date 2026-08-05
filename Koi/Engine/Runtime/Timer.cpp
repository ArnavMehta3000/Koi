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
		m_accumulatedTime = 0.0;
		m_frameCount      = 0;
		m_fps             = 0;
		m_fpsCounter      = 0;
		m_fpsAccumulator  = 0.0;
	}
}
