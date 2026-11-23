#include "Koi/Engine/Graphics/SceneRenderer.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RenderView.h"
#include "Koi/Engine/RHI/Device.h"
#include "Koi/Engine/Graphics/TargetCapture.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	SceneRenderer::SceneRenderer(Renderer& renderer, std::string_view pipeline)
		: m_renderer(&renderer)
	{
		m_graph.Init(renderer.GetDevice(), renderer.GetPipelineCache());
		SetPipeline(pipeline);
	}

	SceneRenderer::~SceneRenderer()
	{
		m_renderer->Dequeue(*this);
		m_renderer->GetDevice().WaitForIdle();  // the GPU may still read this graph's transients
		m_features.Clear();
		m_graph.Shutdown();
	}

	void SceneRenderer::SetPipeline(std::string_view pipeline)
	{
		ZoneScoped;

		const Renderer::PipelineBuildFunc build = m_renderer->FindPipeline(pipeline);
		if (!build)
		{
			KOI_LOG(Render, Error, "No render pipeline named '{}'", pipeline);
			return;
		}

		if (!m_pipeline.empty())
		{
			m_renderer->Dequeue(*this);
			m_renderer->GetDevice().WaitForIdle();
			m_graph.Reset();  // its callbacks point into the features about to go
			m_features.Clear();
		}

		m_pipeline = std::string(pipeline);
		build(*m_renderer, m_features);
	}

	void SceneRenderer::Submit(const RenderView& view, const RenderScene& scene)
	{
		KOI_SCOPE("Render Setup");
		m_graph.Reset();
		if (!view.Camera) return;

		if (m_capture)
		{
			m_capture->SetDepthPlanes(view.Camera->Near, view.Camera->Far);
		}
		m_features.Record(m_graph, view, scene);
		m_renderer->Enqueue(*this);
	}

	void SceneRenderer::SetCapture(TargetCapture* capture)
	{
		m_capture = capture;
		m_graph.SetObserver(capture);
	}

	void SceneRenderer::Execute(CommandList& cmd)
	{
		m_graph.Execute(cmd);
	}
}
