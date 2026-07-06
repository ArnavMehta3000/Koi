#include "Runtime/AppBase.h"
#include "Runtime/Engine.h"
#include "Editor/EditorContext.h"
#include "Graphics/Renderer.h"
#include "Graphics/RenderWindow.h"
#include "Core/Profiling.h"
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

	std::optional<ViewportTarget> AppBase::GetFrameTarget() const
	{
		if (m_editorContext)
		{
			const ViewportTarget* vp = m_editorContext->GetViewportTarget();
			return vp ? std::optional(*vp) : std::nullopt;
		}

		Renderer& renderer = m_engine->GetRenderer();
		RenderWindow* rw = renderer.GetMainRenderWindow();
		if (!rw) return std::nullopt;

		return ViewportTarget
		{
			.HDRColorBuffer      = rw->GetHDRColorBuffer(),
			.DepthBuffer         = renderer.GetDepthBuffer(),
			.ResolvedColorBuffer = rw->CurrentBackBuffer(),
			.Width               = rw->GetWidth(),
			.Height              = rw->GetHeight(),
		};
	}

	const Camera* AppBase::ResolveCamera(const Camera* fallback) const
	{
		if (m_editorContext)
		{
			if (const Camera* editorCamera = m_editorContext->GetEditorCamera())
				return editorCamera;
		}
		return fallback;
	}
}
