#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <utility>

namespace koi
{
	AppBase* AppBase::s_instance = nullptr;

	AppBase& AppBase::Get()
	{
		return *s_instance;
	}

	AppBase::AppBase()
	{
		KOI_ASSERT(s_instance == nullptr, "Application already exists!");
		s_instance = this;
		m_engine = &Engine::Get();
	}

	AppBase::~AppBase()
	{
		s_instance = nullptr;
		m_engine = nullptr;
	}

	void AppBase::HostInit()
	{
		KOI_LOG(Engine, Debug, "Initializing application: {}", m_desc.AppName);

		OnRegisterComponents();
		OnRegisterSystems();
		OnRegisterModules();

		// A World builds its systems on construction (World::BuildSystems)
		//   so it comes after every registration
		m_world = std::make_unique<World>();
		m_world->SetFixedStep(m_desc.FixedStep);
		LoadStartupScene();
		OnInit();
	}

	void AppBase::HostTick(f32 dt)
	{
		m_world->Update(dt);

		{
			KOI_SCOPE("Game Tick");
			OnTick(dt);
		}

		// Nothing is iterating the world here
		if (!m_pendingScene.empty())
		{
			LoadSceneNow(std::exchange(m_pendingScene, {}));
		}
	}

	void AppBase::HostRender()
	{
		HostRenderInto(GetWindowView());
	}

	void AppBase::HostRenderInto(const std::optional<RenderView>& view)
	{
		if (view)
			m_engine->GetSubsystem<Renderer>().GetMainSceneRenderer().Submit(*view, m_world->GetRenderScene());

		OnRender();
	}

	void AppBase::HostShutdown()
	{
		OnShutdown();

		// Hands back the asset references the scene's entities took when they loaded
		m_world->Clear();
		KOI_LOG(Engine, Debug, "Shutting down application: {}", m_desc.AppName);
	}

	void AppBase::HostBeginPlay()
	{
		m_world->BeginPlay();
		OnBeginPlay();
	}

	void AppBase::HostEndPlay()
	{
		OnEndPlay();
		m_world->EndPlay();
	}

	void AppBase::LoadStartupScene()
	{
		ZoneScoped;
		if (m_desc.StartupScene.empty())
		{
			KOI_LOG(Scene, Warn, "No [scenes] startup set in the project TOML");
			return;
		}

		if (!m_desc.Scenes.contains(m_desc.StartupScene))
		{
			KOI_LOG(Scene, Error, "Startup scene '{}' is not listed in [scenes.list]", m_desc.StartupScene);
			return;
		}

		LoadSceneNow(m_desc.StartupScene);
	}

	bool AppBase::LoadScene(std::string_view name)
	{
		if (!m_desc.Scenes.contains(name))
		{
			KOI_LOG(Scene, Error, "Scene '{}' is not listed in [scenes.list]", name);
			return false;
		}

		if (!m_pendingScene.empty() && m_pendingScene != name)
		{
			KOI_LOG(Scene, Warn, "Scene '{}' replaces the pending load of '{}'", name, m_pendingScene);
		}
		KOI_LOG(Scene, Debug, "Queued scene '{}'", name);
		m_pendingScene = name;
		return true;
	}

	void AppBase::LoadSceneNow(const std::string& name)
	{
		ZoneScoped;
		const std::string& path = m_desc.Scenes.at(name);

		// Loaded in place
		if (const Result<void> loaded = m_world->Load(path); !loaded)
		{
			KOI_LOG(Scene, Error, "Couldn't load scene '{}' ({}): {}", name, path, loaded.error().Message);
			return;
		}
		OnSceneLoaded(name);
	}

	std::optional<RenderView> AppBase::GetWindowView() const
	{
		RenderWindow* rw = m_engine->GetSubsystem<Renderer>().GetMainRenderWindow();
		if (!rw || rw->GetHeight() == 0) return std::nullopt;

		const f32 aspect = static_cast<f32>(rw->GetWidth()) / static_cast<f32>(rw->GetHeight());

		// No primary camera in the world (e.g. an empty scene) - nothing to render yet
		const Camera* camera = m_world->ResolveCamera(aspect);
		if (!camera) return std::nullopt;

		return RenderView
		{
			.Camera = camera,
			.Target =
			{
				.ResolvedColorBuffer = rw->CurrentBackBuffer(),
				.Width               = rw->GetWidth(),
				.Height              = rw->GetHeight(),
			},
			.Flags = ViewFlags::DebugDraw,
		};
	}
}
