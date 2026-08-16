#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	AppBase::AppBase(const AppCreateDesc& desc)
		: m_desc(desc)
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
		OnInit();
	}

	void AppBase::HostTick(f32 dt)
	{
		OnTick(dt);
	}

	void AppBase::HostRender()
	{
		OnRender();
	}

	void AppBase::HostShutdown()
	{
		OnShutdown();
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

	std::optional<RenderView> AppBase::GetRenderView(World& world) const
	{
		// Editor owns both the camera and the target (viewport RT)
		if (m_editorContext)
			return m_editorContext->GetEditorRenderView();

		// Standalone: render the world's camera straight into the swapchain
		RenderWindow* rw = m_engine->GetSubsystem<Renderer>().GetMainRenderWindow();
		if (!rw || rw->GetHeight() == 0) return std::nullopt;

		const f32 aspect = static_cast<f32>(rw->GetWidth()) / static_cast<f32>(rw->GetHeight());

		return RenderView
		{
			.Camera = world.ResolveCamera(aspect),
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
