#include "Koi/Engine/Graphics/DeferredPipeline.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	void DeferredPipeline::Build(Renderer& renderer, RenderGraph& graph)
	{
		ZoneScoped;

		m_shadowPass = &graph.AddPass<ShadowPass>(ShadowPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		m_gbufferPass = &graph.AddPass<GBufferPass>(GBufferPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
			.Materials = &renderer.GetMaterialTable(),
		});

		m_lightingPass = &graph.AddPass<LightingPass>(LightingPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		m_tonemapPass = &graph.AddPass<TonemapPass>(TonemapPass::Config
		{
			.Device   = &renderer.GetDevice(),
			.Pipeline = renderer.GetDefaultResources().TonemapPipeline,
		});
	}

	void DeferredPipeline::Setup(const RenderView& view, World& world)
	{
		ZoneScoped;
		const ViewportTarget& target = view.Target;

		// Fits the light matrix up front, so the lighting pass can read it below
		m_shadowPass->SetWorld(&world);

		m_gbufferPass->SetWorld(&world)
			.SetCamera(view.Camera)
			.SetTarget(target.Width, target.Height);

		m_lightingPass->SetWorld(&world)
			.SetCamera(view.Camera)
			.SetGBuffer(m_gbufferPass->GetAlbedo(), m_gbufferPass->GetNormals(), m_gbufferPass->GetMaterial(), m_gbufferPass->GetDepth())
			.SetShadow(m_shadowPass->GetShadowMap(), m_shadowPass->GetLightViewProjection())
			.SetTarget(target.HDRColorBuffer, target.Width, target.Height);

		m_tonemapPass->SetInput(m_lightingPass->GetColorOutput())
			.SetTarget(target.ResolvedColorBuffer, target.Width, target.Height);
	}

	std::unique_ptr<IRenderPipeline> CreateRenderPipeline(std::string_view name)
	{
		if (!name.empty() && name != "deferred")
		{
			KOI_LOG_WARN("Unknown render pipeline '{}', falling back to deferred", name);
		}

		return std::make_unique<DeferredPipeline>();
	}
}
