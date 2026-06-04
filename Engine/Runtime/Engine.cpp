#include "Runtime/Engine.h"
#include "Runtime/AppBase.h"
#include "Graphics/RHI/Device.h"
#include "Memory/MemoryDebug.h"
#include "Core/Profiling.h"
#include <algorithm>
#include <cassert>
#include <ranges>

namespace koi
{
	Engine::Engine()
		: m_initSnapshot(GetMemSnapshot())
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);

		assert(s_instance == nullptr && "Engine already initialized");
		s_instance = this;

		KOI_LOG_INFO("Engine initializing");

		m_windowManager = std::make_unique<WindowManager>();
		m_inputManager  = std::make_unique<InputManager>();
		m_renderer      = std::make_unique<Renderer>();
	}

	Engine::~Engine()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_renderer      = nullptr;
		m_inputManager  = nullptr;
		m_windowManager = nullptr;
		KOI_LOG_INFO("Engine shut down");

		PrintMemSnapshot(DiffSnapshots(m_initSnapshot, GetMemSnapshot()));
		KOI_LOG_DEBUG("Engine was active for {:4f}s", GetTime());
		Logger::Shutdown();

		s_instance = nullptr;
	}

	void Engine::RunApp(AppBase& app)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		const WindowDesc& mainWndDesc = app.GetDesc().WindowDesc;

		auto [windowHandle, windowPtr] = m_windowManager->CreateMainWindow(mainWndDesc);
		m_mainRenderWindow = RegisterWindowWithRenderer(windowHandle, mainWndDesc.VSync);

		app.Init();
		FireHook(EnginePhase::PostInit);
		MainLoop(app);
		FireHook(EnginePhase::PreShutdown);
		app.Shutdown();
	}

	HookId Engine::RegisterHook(EnginePhase phase, EngineHook callback)
	{
		assert(callback && "RegisterHook: callback must not be null");
		const HookId id = m_nextHookId++;
		m_hooks[static_cast<size_t>(phase)].push_back(HookEntry{ .Id = id, .Callback = std::move(callback) });
		return id;
	}
#
	void Engine::UnregisterHook(HookId id)
	{
		if (id == INVALID_HOOK_ID) return;
		for (auto& entries : m_hooks)
		{
			auto it = std::ranges::find_if(entries, [id](const HookEntry& e) { return e.Id == id; });
			if (it != entries.end())
			{
				entries.erase(it);
				return;
			}
		}
	}

	void Engine::FireHook(EnginePhase phase)
	{
		auto& entries = m_hooks[static_cast<size_t>(phase)];
		const size_t n = entries.size();

		for (size_t i = 0; i < n && i < entries.size(); ++i)
		{
			if (entries[i].Callback) entries[i].Callback();
		}
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
			m_inputManager->RegisterWindow(handle, *wnd);

			auto&& [renderHandle, renderWindowPtr] = m_renderer->AddWindow(handle, isVsync);
			renderWndHandle = renderHandle;

			wnd->SetResizeCallback(
				[renderHandle = renderHandle, renderer = m_renderer.get()](u32 w, u32 h)
				{
					if (RenderWindow* rw = renderer->GetRenderWindow(renderHandle))
						rw->Resize(w, h);
				});

			wnd->SetCloseCallback(
				[window = handle, renderer = m_renderer.get(), input = m_inputManager.get()]()
				{
					renderer->RemoveWindow(window);
					input->UnregisterWindow(window);
				});
		}

		return renderWndHandle;
	}

	void Engine::BeginFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_inputManager->BeginFrame();

		Window::PollEvents();
		m_windowManager->DestroyClosedWindows();

		m_renderer->BeginFrame();
	}

	void Engine::Tick(AppBase& app)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		FireHook(EnginePhase::PreTick);
		m_timer.Tick([&app](f32 dt) { app.OnTick(dt); });
		FireHook(EnginePhase::PostTick);
		FireHook(EnginePhase::PreRender);
		app.OnRender();
		FireHook(EnginePhase::PostRender);
	}

	void Engine::EndFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_renderer->EndFrame();
	}

	void Engine::MainLoop(AppBase& app)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_timer.Reset();

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
