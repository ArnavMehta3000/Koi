#include "Koi/Engine/Graphics/RenderPasses/BlitPass.h"
#include "Koi/Engine/Content/Shaders/Interop/PostProcess.hlsli"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
	namespace
	{
		constexpr RasterPipeline kBlit
		{
			.VertexShader = "engine://Content/Shaders/TonemapVS",  // the full-screen triangle
			.PixelShader  = "engine://Content/Shaders/BlitPS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.DepthStencil = DepthStencilDesc::None(),
		};
	}

	void BlitPass::Record(RenderGraph& graph, const RenderView& view, World&)
	{
		ZoneScoped;

		const DisplayColor* display = graph.GetBlackboard().TryGet<DisplayColor>();
		if (!display) return;

		const RGTexture source = display->Color;
		const RGTexture output = graph.ImportTexture(view.Target.ResolvedColorBuffer, "Output");
		if (!output) return;

		graph.AddPass<RasterPass>("Blit",
			[&](RGBuilder& b)
			{
				b.Read(source);
				(void)b.ColorTarget(output);
			},
			[source](RasterContext& ctx)
			{
				if (!ctx.SetPipeline(kBlit)) return;
				ctx.SetRootConstants(BlitRootConstants{ .SourceIndex = ctx.SRV(source) });
				ctx.Draw(3);
			});
	}
}
