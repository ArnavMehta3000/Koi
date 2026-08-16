#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Memory/MemoryDebug.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <angelscript.h>
#include <cassert>

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
		KOI_LOG_DEBUG("Angelscript version: {} - {}", ::asGetLibraryVersion(), asGetLibraryOptions());

		m_systems.ConstructAll();
		KOI_LOG_DEBUG("Engine subsystems constructed");
	}

	Engine::~Engine()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Engine);

		// Runs here, not at the end of RunApp: the host outlives RunApp and releases GPU
		// resources in its own destructor, so the device has to still be alive for it
		GfxDevice& device = GetSubsystem<Renderer>().GetDevice();
		m_systems.ShutdownAll(device);
		KOI_LOG_DEBUG("Engine subsystems shut down");

		m_window.reset();
		KOI_LOG_INFO("Engine shut down after {:.4f}s ({} frames)", GetStats().TotalTime, GetStats().FrameCount);

		PrintMemSnapshot(DiffSnapshots(m_initSnapshot, GetMemSnapshot()));
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

		SubsystemInitContext initCtx
		{
			.MainWindow     = window,
			.VSync          = mainWndDesc.VSync,
			.RenderPipeline = desc.RenderPipeline,
		};
		m_systems.InitAll(initCtx);
		KOI_LOG_DEBUG("Engine subsystems initialized");

		window.SetCloseCallback([this]() { RequestExit(); });

		// Init application
		m_host = &host;
		host.HostInit();
		GetSubsystem<Renderer>().GetFrameGraph().Compile(); // One time validation of the pipeline's pass set, plus anything the game added

		KOI_LOG_INFO("Entering main loop");

#if !KOI_WITH_EDITOR
		// Nothing to press Play with, so a standalone build simulates from the start
		SetPlayState(PlayState::Playing);
#endif

		MainLoop(host);
		KOI_LOG_INFO("Exited main loop");

		SetPlayState(PlayState::Edit);  // fires HostEndPlay when we exit mid-play
		host.HostShutdown();
		m_host = nullptr;
	}

	void Engine::SetPlayState(PlayState state)
	{
		if (state == m_playState) return;

		// Pause/Resume stay inside play mode, so only the Edit boundary is a transition
		const bool wasInPlay = m_playState != PlayState::Edit;
		const bool nowInPlay = state != PlayState::Edit;

		KOI_LOG_INFO("Play state: {} -> {}", ToString(m_playState), ToString(state));
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
		GetSubsystem<InputManager>().BeginFrame();
		Window::PollEvents();
		GetSubsystem<Renderer>().BeginFrame();
	}

	void Engine::Tick(IAppHost& host)
	{
		ZoneScoped;
		// Fallback attribution for tick work: game code pushes its own tag on top
		KOI_MEM_TAG_SCOPE(MemTag::Engine);
		m_timer.Tick([&host](f32 dt) { host.HostTick(dt); });
		host.HostRender();
	}

	void Engine::EndFrame()
	{
		ZoneScoped;
		GetSubsystem<Renderer>().EndFrame();
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
