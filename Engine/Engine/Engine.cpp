#include "Engine/Engine.h"
#include "Application/AppBase.h"

namespace koi
{
	Engine::Engine([[maybe_unused]] i32 argc, [[maybe_unused]] char** argv)
	{
		KOI_ASSERT_MSG(s_instance == nullptr, "Engine already initialized");
		s_instance = this;

		Logger::Init();
		KOI_LOG_INFO("Engine initializing");

		m_windowManager = std::make_unique<WindowManager>();
	}

	Engine::~Engine()
	{
		m_windowManager = nullptr;
		KOI_LOG_INFO("Engine shut down");
		Logger::Shutdown();
		s_instance = nullptr;
	}

	void Engine::RunApp(AppBase& app)
	{
		m_windowManager->CreateMainWindow(app.GetDesc().WindowDesc);

		app.Init();

		m_startTime     = Clock::now();
		m_lastFrameTime = m_startTime;

		while (m_isRunning)
		{
			BeginFrame();
			Tick(app);
			EndFrame();

			if (m_windowManager->AllClosed())
				m_isRunning = false;
		}

		app.Shutdown();
	}

	void Engine::BeginFrame()
	{
		TimePoint now   = Clock::now();
		m_deltaTime     = std::chrono::duration<f32>(now - m_lastFrameTime).count();
		m_elapsed       = std::chrono::duration<f64>(now - m_startTime).count();
		m_lastFrameTime = now;

		Window::PollEvents();
		m_windowManager->DestroyClosedWindows();
	}

	void Engine::Tick(AppBase& app)
	{
		app.OnTick(m_deltaTime);
		app.OnRender();
	}

	void Engine::EndFrame()
	{
		++m_frameCount;
		// Present/ Swap Buffers
	}
}
