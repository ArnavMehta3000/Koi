#include "Koi/Engine/Graphics/DeferredPipeline.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RenderPasses/DebugDrawPass.h"
#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include "Koi/Engine/Logging/Logger.h"

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
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
			.Materials = &renderer.GetMaterialTable(),
		});

		m_lightingPass = &passes.Add<LightingPass>(LightingPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		m_tonemapPass = &passes.Add<TonemapPass>(TonemapPass::Config
		{
			.Device   = &renderer.GetDevice(),
			.Pipeline = renderer.GetDefaultResources().TonemapPipeline,
		});

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

		m_gridPass->SetCamera(view.Camera)
			.SetTarget(target.ResolvedColorBuffer, m_gbufferPass->GetDepth(), target.Width, target.Height);

		m_debugDrawPass->SetCamera(view.Camera)
			.SetTarget(target.ResolvedColorBuffer, m_gbufferPass->GetDepth(), target.Width, target.Height);
	}
}
