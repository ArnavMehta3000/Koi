#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	AppBase::AppBase()
		: m_world(std::make_unique<World>())
	{
		assert(s_instance == nullptr && "Application already exists!");
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
		KOI_LOG_INFO("Initializing application: {}", m_desc.AppName);

		OnRegisterComponents();

		LoadStartupScene();
		OnInit();
	}

	void AppBase::HostTick(f32 dt)
	{
		m_world->Update(dt);

		OnTick(dt);
	}

	void AppBase::HostRender()
	{
		if (const std::optional<RenderView> view = GetRenderView())
			m_engine->GetSubsystem<Renderer>().SetupFrame(*view, *m_world);

		OnRender();
	}

	void AppBase::HostShutdown()
	{
		OnShutdown();

		// Hands back the asset references the scene's entities took when they loaded
		m_world->Clear();
		KOI_LOG_INFO("Shutting down application: {}", m_desc.AppName);
	}

	void AppBase::HostBeginPlay()
	{
		OnBeginPlay();
	}

	void AppBase::HostEndPlay()
	{
		OnEndPlay();
	}

	void AppBase::LoadStartupScene()
	{
		ZoneScoped;
		if (m_desc.StartupScene.empty())
		{
			KOI_LOG_WARN("No startup_scene set in the project TOML");
			return;
		}

		if (std::optional<World> loaded = World::Load(m_desc.StartupScene))
		{
			*m_world = std::move(*loaded);
		}
	}

	std::optional<RenderView> AppBase::GetRenderView() const
	{
		// Editor owns both the camera and the target (viewport RT)
		if (m_editorContext)
			return m_editorContext->GetEditorRenderView();

		// Standalone: render the world's camera straight into the swapchain
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
				.HDRColorBuffer      = rw->GetHDRColorBuffer(),
				.ResolvedColorBuffer = rw->CurrentBackBuffer(),
				.Width               = rw->GetWidth(),
				.Height              = rw->GetHeight(),
			},
		};
	}
}
