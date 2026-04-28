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
		const WindowDesc& mainWndDesc = app.GetDesc().WindowDesc;

		auto [windowHandle, windowPtr] = m_windowManager->CreateMainWindow(mainWndDesc);
		RegisterWindowWithRenderer(windowHandle, mainWndDesc.VSync);

		app.Init();
		MainLoop(app);
		app.Shutdown();
	}

	std::pair<WindowHandle, RenderWindowHandle> Engine::CreateAndAddRenderWindow(const WindowDesc& desc)
	{
		auto&& [windowHandle, windowPtr] = m_windowManager->CreateWindow(desc);
		RenderWindowHandle renderHandle = RegisterWindowWithRenderer(windowHandle, desc.VSync);
		return { windowHandle, renderHandle };
	}

	RenderWindowHandle Engine::RegisterWindowWithRenderer(WindowHandle handle, bool isVsync)
	{
		RenderWindowHandle renderWndHandle;
		if (Window* wnd = m_windowManager->GetWindow(handle))
		{
			auto&& [renderHandle, renderWindowPtr] = m_renderer->AddWindow(handle, isVsync);
			renderWndHandle = renderHandle;

			wnd->SetResizeCallback(
				[renderHandle = renderHandle, renderer = m_renderer.get()](u32 w, u32 h)
				{
					if (RenderWindow* rw = renderer->GetRenderWindow(renderHandle))
						rw->Resize(w, h);
				});

			wnd->SetCloseCallback(
				[window = handle, renderer = m_renderer.get()]()
				{
					renderer->RemoveWindow(window);
				});
		}

		return renderWndHandle;
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
