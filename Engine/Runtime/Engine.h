#pragma once
#include "Core/Core.h"
#include "Platform/WindowManager.h"
#include "Graphics/Renderer.h"
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

		[[nodiscard]] inline WindowManager& GetWindowManager() { return *m_windowManager; }
		[[nodiscard]] inline Renderer& GetRenderer() { return *m_renderer; }
		[[nodiscard]] inline f32 GetDeltaTime() const { return m_deltaTime; }
		[[nodiscard]] inline f64 GetTime() const { return m_elapsed; }
		[[nodiscard]] inline u64 GetFrameCount() const { return m_frameCount; }
		[[nodiscard]] inline RenderWindowHandle GetMainRenderWindowHandle() const { return m_mainRenderWindow; }

		std::pair<WindowHandle, RenderWindowHandle> CreateAndAddRenderWindow(const WindowDesc& desc);
		RenderWindowHandle RegisterWindowWithRenderer(WindowHandle handle, bool isVsync);

	private:
		void BeginFrame();
		void Tick(AppBase& app);
		void EndFrame();
		void MainLoop(AppBase& app);

	private:
		static inline Engine* s_instance = nullptr;

		std::unique_ptr<WindowManager> m_windowManager;
		std::unique_ptr<Renderer> m_renderer;

		using Clock = std::chrono::steady_clock;
		using TimePoint = Clock::time_point;

		TimePoint m_startTime{};
		TimePoint m_lastFrameTime{};
		f32       m_deltaTime  = 0.0f;
		f64       m_elapsed    = 0.0;
		u64       m_frameCount = 0;
		bool               m_isRunning       = true;
		RenderWindowHandle m_mainRenderWindow;
	};
}
