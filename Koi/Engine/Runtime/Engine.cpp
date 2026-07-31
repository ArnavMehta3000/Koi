#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/Format.h"
#include "Koi/Engine/Memory/MemoryDebug.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include <cassert>
#include <angelscript.h>

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
		KOI_LOG_INFO("Angelscript version: {} - {}", ::asGetLibraryVersion(), asGetLibraryOptions());

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
		KOI_LOG_DEBUG("Engine was active for {:4f}s", GetStats().TotalTime);
		Logger::Shutdown();

		s_instance = nullptr;
	}

	void Engine::RunApp(IAppHost& host)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		const AppCreateDesc& desc = host.GetDesc();
		const WindowDesc& mainWndDesc = desc.WindowDesc;

		m_window.emplace(mainWndDesc);
		Window& window = *m_window;

		host.SetWindow(window);

		m_inputManager->Init(window);

		m_renderer->InitMainWindow(window, mainWndDesc.VSync);

		window.SetCloseCallback([this]() { RequestExit(); });

		// Init application
		m_host = &host;
		host.HostInit();
		m_renderer->GetFrameGraph().Compile(); // One time compile to ensure user-defined render pass succeeds post-init

#if !KOI_WITH_EDITOR
		// Nothing to press Play with, so a standalone build simulates from the start
		SetPlayState(PlayState::Playing);
#endif

		MainLoop(host);

		SetPlayState(PlayState::Edit);  // fires HostEndPlay when we exit mid-play
		host.HostShutdown();
		m_host = nullptr;

		m_assets.ReleaseAll(m_renderer->GetDevice());
	}

	void Engine::SetPlayState(PlayState state)
	{
		if (state == m_playState) return;

		// Pause/Resume stay inside play mode, so only the Edit boundary is a transition
		const bool wasInPlay = m_playState != PlayState::Edit;
		const bool nowInPlay = state != PlayState::Edit;

		m_playState = state;

		if (!m_host) return;

		if (!wasInPlay && nowInPlay)
			m_host->HostBeginPlay();
		else if (wasInPlay && !nowInPlay)
			m_host->HostEndPlay();
	}

	Engine::Stats Engine::GetStats() const
	{
		return Stats
		{
			.DeltaTime  = m_timer.GetDeltaTime(),
			.FPS        = m_timer.GetFPS(),
			.FrameCount = m_timer.GetFrameCount(),
			.TotalTime  = m_timer.GetTotalTime(),
		};
	}

	void Engine::BeginFrame()
	{
		ZoneScoped;
		m_inputManager->BeginFrame();
		Window::PollEvents();
		m_renderer->BeginFrame();
	}

	void Engine::Tick(IAppHost& host)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);  // Maybe remove this tag?
		m_timer.Tick([&host](f32 dt) { host.HostTick(dt); });
		host.HostRender();
	}

	void Engine::EndFrame()
	{
		ZoneScoped;
		m_renderer->EndFrame();
	}

	void Engine::MainLoop(IAppHost& host)
	{
		m_timer.Reset();

		while (m_isRunning)
		{
			BeginFrame();
			Tick(host);
			EndFrame();
			FrameMark;
		}
	}
}
