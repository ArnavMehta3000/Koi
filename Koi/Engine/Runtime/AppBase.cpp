#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
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
		KOI_LOG(Engine, Info, "Initializing application: {}", m_desc.AppName);

		m_world = std::make_unique<World>();

		OnRegisterComponents();
		OnRegisterSystems();

		LoadStartupScene();
		OnInit();
	}

	void AppBase::HostTick(f32 dt)
	{
		m_world->Update(dt);

		KOI_SCOPE("Game Tick");
		OnTick(dt);
	}

	void AppBase::HostRender()
	{
		HostRenderInto(GetWindowView());
	}

	void AppBase::HostRenderInto(const std::optional<RenderView>& view)
	{
		if (view)
			m_engine->GetSubsystem<Renderer>().SetupFrame(*view, *m_world);

		OnRender();
	}

	void AppBase::HostShutdown()
	{
		OnShutdown();

		// Hands back the asset references the scene's entities took when they loaded
		m_world->Clear();
		KOI_LOG(Engine, Info, "Shutting down application: {}", m_desc.AppName);
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

		LoadScene(m_desc.StartupScene);
	}

	bool AppBase::LoadScene(std::string_view name)
	{
		ZoneScoped;

		const auto it = m_desc.Scenes.find(std::string(name));
		if (it == m_desc.Scenes.end())
		{
			KOI_LOG(Scene, Error, "Scene '{}' is not listed in [scenes.list]", name);
			return false;
		}

		Result<World> loaded = World::Load(it->second);
		if (!loaded)
		{
			KOI_LOG(Scene, Error, "Couldn't load scene '{}' ({}): {}", name, it->second, loaded.error().Message);
			return false;
		}

		const bool inPlay = m_engine->GetPlayState() != PlayState::Edit;
		if (inPlay) m_world->EndPlay();

		*m_world = std::move(*loaded);

		if (inPlay) m_world->BeginPlay();
		return true;
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
		};
	}
}
