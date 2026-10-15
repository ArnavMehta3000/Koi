#include "Koi/Engine/Graphics/DeferredPipeline.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RenderPasses/BlitPass.h"
#include "Koi/Engine/Graphics/RenderPasses/DebugDrawPass.h"
#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Interop/ViewportTarget.h"

namespace koi
{
	void DeferredPipeline::Build(Renderer& renderer, PassList& passes)
	{
		ZoneScoped;

		m_shadowPass = &passes.Add<ShadowPass>(ShadowPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		m_gbufferPass = &passes.Add<GBufferPass>(GBufferPass::Config
		{
			.Device          = &renderer.GetDevice(),
			.Resources       = &renderer.GetDefaultResources(),
			.Materials       = &renderer.GetMaterialTable(),
			.DefaultPipeline = &renderer.GetMaterialTemplates().Default()->SingleSided,
		});

		m_lightingPass = &passes.Add<LightingPass>(LightingPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		m_tonemapPass = &passes.Add<TonemapPass>();

		// Over the tonemapped image, under the debug lines
		m_gridPass = &passes.Add<GridPass>(GridPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		m_debugDrawPass = &passes.Add<DebugDrawPass>(DebugDrawPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
			.Queue     = &renderer.GetDebugDraw(),
		});

		m_blitPass = &passes.Add<BlitPass>();
	}

	void DeferredPipeline::Record(RenderGraph& graph, const RenderView& view, World& world)
	{
		ZoneScoped;
		if (!view.Camera) return;

		const Camera&         camera = *view.Camera;
		const ViewportTarget& target = view.Target;

		m_shadowPass->Record(graph, world);
		m_gbufferPass->Record(graph, world, camera, target.Width, target.Height);
		m_lightingPass->Record(graph, world, camera);
		m_tonemapPass->Record(graph);

		// Over the tonemapped image, under the debug lines
		m_gridPass->Record(graph, camera);
		m_debugDrawPass->Record(graph, camera);

		m_blitPass->Record(graph, graph.ImportTexture(target.ResolvedColorBuffer, "Output"));
	}
}
