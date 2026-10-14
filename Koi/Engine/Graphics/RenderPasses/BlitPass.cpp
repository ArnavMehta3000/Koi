#include "Koi/Engine/Graphics/RenderPasses/BlitPass.h"
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

	RGTexture BlitPass::Record(RenderGraph& graph, RGTexture source, RGTexture output)
	{
		ZoneScoped;
		if (!output) return output;

		graph.AddPass<RasterPass>("Blit",
			[&](RGBuilder& b)
			{
				b.Read(source);
				output = b.ColorTarget(output);
			},
			[source](RasterContext& ctx)
			{
				if (!ctx.SetPipeline(kBlit)) return;
				ctx.SetRootConstants(ctx.SRV(source));
				ctx.Draw(3);
			});

		return output;
	}
}
