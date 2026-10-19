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

namespace koi
{
	void DeferredPipeline::Build(Renderer& renderer, FeatureList& features)
	{
		ZoneScoped;

		features.Add<ShadowPass>(ShadowPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		features.Add<GBufferPass>(GBufferPass::Config
		{
			.Device          = &renderer.GetDevice(),
			.Resources       = &renderer.GetDefaultResources(),
			.Materials       = &renderer.GetMaterialTable(),
			.DefaultPipeline = &renderer.GetMaterialTemplates().Default()->SingleSided,
		});

		features.Add<LightingPass>(LightingPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		features.Add<TonemapPass>();

		// Over the tonemapped image, under the debug lines
		features.Add<GridPass>(GridPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
		});

		features.Add<DebugDrawPass>(DebugDrawPass::Config
		{
			.Device    = &renderer.GetDevice(),
			.Resources = &renderer.GetDefaultResources(),
			.Queue     = &renderer.GetDebugDraw(),
		});

		features.Add<BlitPass>();
	}
}
