#include "Engine/Engine.h"
#include "Application/AppBase.h"
#include "Graphics/RHI/Device.cpp"
#include "Memory/MemoryDebug.h"
#include "Core/Profiling.h"
#include <cassert>

namespace koi
{
	MemTagSnapshotArray onInitSnapshot;

	Engine::Engine([[maybe_unused]] i32 argc, [[maybe_unused]] char** argv)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);

		assert(s_instance == nullptr && "Engine already initialized");
		s_instance = this;

		onInitSnapshot = GetMemSnapshot();

		Logger::Init();
		KOI_LOG_INFO("Engine initializing");

		m_windowManager = std::make_unique<WindowManager>();
		m_renderer = std::make_unique<Renderer>();
	}

	Engine::~Engine()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_renderer = nullptr;
		m_windowManager = nullptr;
		KOI_LOG_INFO("Engine shut down");

		PrintMemSnapshot(DiffSnapshots(onInitSnapshot, GetMemSnapshot()));

		Logger::Shutdown();
		s_instance = nullptr;
	}

	void Engine::RunApp(AppBase& app)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		const WindowDesc& mainWndDesc = app.GetDesc().WindowDesc;

		auto [windowHandle, windowPtr] = m_windowManager->CreateMainWindow(mainWndDesc);
		m_mainRenderWindow = RegisterWindowWithRenderer(windowHandle, mainWndDesc.VSync);

		app.Init();
		MainLoop(app);
		app.Shutdown();
	}

	std::pair<WindowHandle, RenderWindowHandle> Engine::CreateAndAddRenderWindow(const WindowDesc& desc)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		auto&& [windowHandle, windowPtr] = m_windowManager->CreateWindow(desc);
		RenderWindowHandle renderHandle = RegisterWindowWithRenderer(windowHandle, desc.VSync);
		return { windowHandle, renderHandle };
	}

	RenderWindowHandle Engine::RegisterWindowWithRenderer(WindowHandle handle, bool isVsync)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
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
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
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
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		app.OnTick(m_deltaTime);
		app.OnRender();
	}

	void Engine::EndFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_renderer->EndFrame();
		m_frameCount = m_renderer->GetDevice().GetCurrentFrameIndex();
	}

	void Engine::MainLoop(AppBase& app)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_startTime = Clock::now();
		m_lastFrameTime = m_startTime;

		while (m_isRunning)
		{
			BeginFrame();
			Tick(app);
			EndFrame();

			if (m_windowManager->AllClosed())
				m_isRunning = false;

			FrameMark;
		}
	}
}
