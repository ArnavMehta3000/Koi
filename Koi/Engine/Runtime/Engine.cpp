#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Resources/AssetRegistry.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/RHI/Device.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/AppHost.h"

namespace koi
{
	template<> InputManager&  Engine::GetSubsystem<InputManager>()  { return *m_input;    }
	template<> Renderer&      Engine::GetSubsystem<Renderer>()      { return *m_renderer; }
	template<> AssetRegistry& Engine::GetSubsystem<AssetRegistry>() { return *m_assets;   }

	Engine* Engine::s_instance = nullptr;

	Engine& Engine::Get()
	{
		return *s_instance;
	}

	Engine::Engine(const Desc& desc)
		: m_desc(desc)
	{
		ZoneScoped;

		KOI_ASSERT(s_instance == nullptr, "Engine already initialized");
		s_instance = this;

		KOI_LOG(Engine, Info, "Engine initializing");

		m_input    = std::make_unique<InputManager>();
		m_renderer = std::make_unique<Renderer>();
		m_assets   = std::make_unique<AssetRegistry>();
		KOI_LOG(Engine, Debug, "Engine subsystems constructed");

		ComponentRegistry::Get().RegisterEngineComponents();
	}

	Engine::~Engine()
	{
		ZoneScoped;

		// Runs here, not at the end of RunApp: the host outlives RunApp and releases GPU
		// resources in its own destructor, so the device has to still be alive for it
		m_assets->Shutdown(m_renderer->GetDevice());
		m_assets.reset();
		m_renderer.reset();
		m_input.reset();
		KOI_LOG(Engine, Debug, "Engine subsystems shut down");

		m_window.reset();
		KOI_LOG(Engine, Info, "Engine shut down after {:.4f}s ({} frames)", GetStats().TotalTime, GetStats().FrameCount);

		s_instance = nullptr;
	}

	void Engine::RunApp(IAppHost& host)
	{
		const AppCreateDesc& desc = host.GetDesc();
		const WindowDesc& mainWndDesc = desc.WindowDesc;

		m_window = std::make_unique<Window>(mainWndDesc);
		Window& window = *m_window;

		host.SetWindow(window);

		m_input->Init(window, desc.Input);
		m_renderer->Init(window, mainWndDesc.VSync);
		m_assets->Init();
		KOI_LOG(Engine, Debug, "Engine subsystems initialized");

		window.SetCloseCallback([this]() { RequestExit(); });

		// Init application
		m_host = &host;
		host.HostInit();

		KOI_LOG(Engine, Info, "Entering main loop");

		if (host.StartsInPlayMode())
		{
			SetPlayState(PlayState::Playing);
		}

		MainLoop(host);
		KOI_LOG(Engine, Info, "Exited main loop");

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

		KOI_LOG(Engine, Info, "Play state: {} -> {}", ToString(m_playState), ToString(state));
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
		{
			KOI_SCOPE("Input");
			m_input->BeginFrame(IsPlaying());
			Window::PollEvents();
		}
		GetSubsystem<Renderer>().BeginFrame();
	}

	void Engine::Tick(IAppHost& host)
	{
		ZoneScoped;
		m_timer.Tick();
		host.HostTick(m_timer.GetDeltaTime());
		host.HostRender();
	}

	void Engine::EndFrame()
	{
		ZoneScoped;
		Renderer& renderer = GetSubsystem<Renderer>();
		renderer.EndFrame();
		GetSubsystem<AssetRegistry>().Collect(renderer.GetDevice(), m_timer.GetTotalTime());
	}

	void Engine::MainLoop(IAppHost& host)
	{
		m_timer.Reset();

		while (m_isRunning)
		{
			FrameStats::NewFrame();
			BeginFrame();
			Tick(host);
			EndFrame();
			FrameMark;

			if (m_desc.Timeout > 0.0 && m_timer.GetTotalTime() >= m_desc.Timeout)
			{
				KOI_LOG(Engine, Info, "--timeout {}s reached, exiting", m_desc.Timeout);
				RequestExit();
			}
		}
	}
}
