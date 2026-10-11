#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
	namespace
	{
		constexpr RasterPipeline kTonemap
		{
			.VertexShader = "engine://Content/Shaders/TonemapVS",
			.PixelShader  = "engine://Content/Shaders/TonemapPS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.DepthStencil = DepthStencilDesc::None(),
		};
	}

	RGTexture TonemapPass::Record(RenderGraph& graph, RGTexture hdr, RGTexture output)
	{
		ZoneScoped;
		if (!output) return output;

		graph.AddPass<RasterPass>("Tonemap",
			[&](RGBuilder& b)
			{
				b.Read(hdr);
				output = b.ColorTarget(output);
			},
			[hdr](RasterContext& ctx)
			{
				if (!ctx.SetPipeline(kTonemap)) return;
				ctx.SetRootConstants(ctx.SRV(hdr));
				ctx.Draw(3);
			});

		return output;
	}
}
