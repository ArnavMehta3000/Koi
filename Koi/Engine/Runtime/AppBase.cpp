#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include <cassert>

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

	f32 AppBase::GetRenderAspect() const
	{
		// No camera needed: only the target half of the view is being asked about
		const std::optional<RenderView> view = GetRenderView(nullptr);
		if (!view || view->Target.Height == 0) return 1.0f;

		return static_cast<f32>(view->Target.Width) / static_cast<f32>(view->Target.Height);
	}

	std::optional<RenderView> AppBase::GetRenderView(const Camera* gameCamera) const
	{
		// Editor owns both the camera and the target (viewport RT)
		if (m_editorContext)
			return m_editorContext->GetEditorRenderView();

		// Standalone: render from the game camera straight into the swapchain
		Renderer& renderer = m_engine->GetRenderer();
		RenderWindow* rw = renderer.GetMainRenderWindow();
		if (!rw) return std::nullopt;

		return RenderView
		{
			.Camera = gameCamera,
			.Target =
			{
				.HDRColorBuffer      = rw->GetHDRColorBuffer(),
				.DepthBuffer         = renderer.GetDepthBuffer(),
				.ResolvedColorBuffer = rw->CurrentBackBuffer(),
				.Width               = rw->GetWidth(),
				.Height              = rw->GetHeight(),
			},
		};
	}
}
