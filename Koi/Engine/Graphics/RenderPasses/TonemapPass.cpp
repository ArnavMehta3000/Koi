#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
	RGTexture TonemapPass::Record(RenderGraph& graph, RGTexture hdr, RGTexture output)
	{
		ZoneScoped;
		if (!m_config.Pipeline || !output) return output;

		graph.AddPass<RasterPass>("Tonemap",
			[&](RGBuilder& b)
			{
				b.Read(hdr);
				output = b.ColorTarget(output);
			},
			[this, hdr](RasterContext& ctx)
			{
				ctx.SetPipeline(m_config.Pipeline);
				ctx.SetRootConstants(ctx.SRV(hdr));
				ctx.Draw(3);
			});

		return output;
	}
}
