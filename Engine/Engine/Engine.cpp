#include "Engine/Engine.h"
#include "Application/AppBase.h"
#include "Graphics/RHI/Device.cpp"

namespace koi
{
	Engine::Engine([[maybe_unused]] i32 argc, [[maybe_unused]] char** argv)
	{
		KOI_ASSERT_MSG(s_instance == nullptr, "Engine already initialized");
		s_instance = this;

		Logger::Init();
		KOI_LOG_INFO("Engine initializing");

		m_windowManager = std::make_unique<WindowManager>();
		m_renderer = std::make_unique<Renderer>();
	}

	Engine::~Engine()
	{
		m_renderer = nullptr;
		m_windowManager = nullptr;
		KOI_LOG_INFO("Engine shut down");
		Logger::Shutdown();
		s_instance = nullptr;
	}

	void Engine::RunApp(AppBase& app)
	{
		// Create main window, register with renderer and set up callbacks
		const WindowDesc& mainWndDesc             = app.GetDesc().WindowDesc;
		const WindowHandle mainWindow             = m_windowManager->CreateMainWindow(mainWndDesc);
		const RenderWindowHandle mainRenderWindow = m_renderer->AddWindow(mainWindow, mainWndDesc.VSync);

		m_windowManager->GetMain()->SetCloseCallback(
			[window = mainWindow, renderer = m_renderer.get()]()
			{
				renderer->RemoveWindow(window);
			});

		m_windowManager->GetMain()->SetResizeCallback(
			[rwHandle = mainRenderWindow, renderer = m_renderer.get()](u32 w, u32 h)
			{
				if (RenderWindow* rw = renderer->GetRenderWindow(rwHandle))
					rw->Resize(w, h);
			});

		app.Init();
		MainLoop(app);
		app.Shutdown();
	}

	std::pair<WindowHandle, RenderWindowHandle> Engine::CreateAndAddRenderWindow(const WindowDesc& desc)
	{
		WindowHandle window = m_windowManager->Create(desc);
		RenderWindowHandle renderWindow = m_renderer->AddWindow(window, desc.VSync);

		// Set up callbacks
		if (Window* wnd = m_windowManager->Get(window))
		{
			wnd->SetCloseCallback(
				[window = window, renderer = m_renderer.get()]()
				{
					renderer->RemoveWindow(window);
				});

			m_windowManager->Get(window)->SetResizeCallback(
				[rwHandle = renderWindow, renderer = m_renderer.get()](u32 w, u32 h)
				{
					if (RenderWindow* rw = renderer->GetRenderWindow(rwHandle)) rw->Resize(w, h);
				});
		}


		return { window, renderWindow };
	}

	void Engine::BeginFrame()
	{
		TimePoint now   = Clock::now();
		m_deltaTime     = std::chrono::duration<f32>(now - m_lastFrameTime).count();
		m_elapsed       = std::chrono::duration<f64>(now - m_startTime).count();
		m_lastFrameTime = now;

		Window::PollEvents();
		m_windowManager->DestroyClosedWindows();

		m_renderer->BeginFrame();
	}

	void Engine::Tick(AppBase& app)
	{
		app.OnTick(m_deltaTime);
		app.OnRender();
	}

	void Engine::EndFrame()
	{
		m_renderer->EndFrame();
		m_frameCount = m_renderer->GetDevice().GetCurrentFrameIndex();
	}

	void Engine::MainLoop(AppBase& app)
	{
		m_startTime = Clock::now();
		m_lastFrameTime = m_startTime;

		while (m_isRunning)
		{
			BeginFrame();
			Tick(app);
			EndFrame();

			if (m_windowManager->AllClosed())
				m_isRunning = false;
		}
	}
}
