#include "Runtime/Engine.h"
#include "Runtime/AppBase.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/Format.h"
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

		m_inputManager = std::make_unique<InputManager>();
		m_renderer     = std::make_unique<Renderer>();
	}

	Engine::~Engine()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_renderer     = nullptr;
		m_inputManager = nullptr;
		m_window.reset();
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

		m_window.emplace(mainWndDesc);
		Window& window = *m_window;

		app.m_window = &window;

		m_inputManager->Init(window);

		const Format depthFmt = [&]() -> Format {
			switch (app.GetDesc().DepthBuffer)
			{
				case DepthBufferFormat::D16:   return Format::D16_UNORM;
				case DepthBufferFormat::D24S8:  return Format::D24_UNORM_S8_UINT;
				case DepthBufferFormat::None:   return Format::UNKNOWN;
				default:                        return Format::D32_FLOAT;
			}
		}();

		m_renderer->InitMainWindow(window, mainWndDesc.VSync, depthFmt);

		window.SetCloseCallback([this]() { RequestExit(); });

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

	void Engine::BeginFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_inputManager->BeginFrame();
		Window::PollEvents();
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
			FrameMark;
		}
	}
}
