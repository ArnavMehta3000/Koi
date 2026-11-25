#include "Koi/Engine/Graphics/DeferredPipeline.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RenderPasses/AutoExposurePass.h"
#include "Koi/Engine/Graphics/RenderPasses/BlitPass.h"
#include "Koi/Engine/Graphics/RenderPasses/DebugDrawPass.h"
#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Graphics/RenderPasses/SkyPass.h"
#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Graphics/Renderer.h"

namespace koi
{
	void DeferredPipeline::Build(Renderer& renderer, FeatureList& features)
	{
		ZoneScoped;

		GfxDevice* device = &renderer.GetDevice();
		RendererResources* defResources = &renderer.GetDefaultResources();
		MaterialTable* matTable = &renderer.GetMaterialTable();

		features.Add<ShadowPass>(ShadowPass::Config
		{
			.Device    = device,
			.Resources = defResources,
			.Materials = matTable,
		});

		features.Add<GBufferPass>(GBufferPass::Config
		{
			.Device          = device,
			.Resources       = defResources,
			.Materials       = matTable,
			.DefaultPipeline = &renderer.GetMaterialTemplates().Default()->PipelineFor(false, false),
		});

		features.Add<LightingPass>(LightingPass::Config
		{
			.Device    = device,
			.Resources = defResources,
		});

		features.Add<SkyPass>(SkyPass::Config
		{
			.Resources = defResources,
		});

		features.Add<AutoExposurePass>(AutoExposurePass::Config
		{
			.Device = device,
		});
		features.Add<TonemapPass>();

#if KOI_WITH_EDITOR
		// Over the tonemapped image, under the debug lines
		features.Add<GridPass>(GridPass::Config
		{
			.Device    = device,
			.Resources = defResources,
		});
#endif

#if KOI_DEBUG_DRAW
		features.Add<DebugDrawPass>(DebugDrawPass::Config
		{
			.Device    = device,
			.Resources = defResources,
			.Queue     = &renderer.GetDebugDraw(),
		});
#endif

		features.Add<BlitPass>();
	}
}
