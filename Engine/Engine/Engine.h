#pragma once
#include "Core/Core.h"
#include "Application/WindowManager.h"
#include <chrono>

namespace koi
{
	class AppBase;

	class Engine : public NonCopyable
	{
	public:
		explicit Engine(i32 argc, char** argv);
		~Engine();

		[[nodiscard]] static Engine& Get() { return *s_instance; }

		void RunApp(AppBase& app);
		void RequestExit() { m_isRunning = false; }

		[[nodiscard]] WindowManager& GetWindowManager() { return *m_windowManager; }
		[[nodiscard]] f32            GetDeltaTime() const { return m_deltaTime; }
		[[nodiscard]] f64            GetTime()      const { return m_elapsed; }
		[[nodiscard]] u64            GetFrameCount() const { return m_frameCount; }

	private:
		void BeginFrame();
		void Tick(AppBase& app);
		void EndFrame();

	private:
		static inline Engine* s_instance = nullptr;

		std::unique_ptr<WindowManager> m_windowManager;

		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		TimePoint m_startTime{};
		TimePoint m_lastFrameTime{};
		f32       m_deltaTime  = 0.0f;
		f64       m_elapsed    = 0.0;
		u64       m_frameCount = 0;
		bool      m_isRunning  = true;
	};
}
