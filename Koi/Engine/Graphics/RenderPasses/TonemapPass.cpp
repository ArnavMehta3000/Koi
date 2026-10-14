#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
	namespace
	{
		constexpr ComputePipeline kTonemap
		{
			.ComputeShader = "engine://Content/Shaders/TonemapCS",
		};

		struct TonemapRootConstants
		{
			DescriptorIndex HDRTextureIndex;
			DescriptorIndex LDRTextureIndex;
		};
	}

	RGTexture TonemapPass::Record(RenderGraph& graph, RGTexture hdr, RGTexture output)
	{
		ZoneScoped;
		if (!output) return output;

		const TextureDesc& desc = graph.GetDesc(output);

		graph.AddPass<ComputePass>("Tonemap",
			[&](RGBuilder& b)
			{
				b.Read(hdr);
				output = b.Write(output);
			},
			[hdr, ldr = output, width = desc.Width, height = desc.Height](ComputeContext& ctx)
			{
				if (!ctx.SetPipeline(kTonemap)) return;
				ctx.SetRootConstants(TonemapRootConstants{ .HDRTextureIndex = ctx.SRV(hdr), .LDRTextureIndex = ctx.UAV(ldr) });

				// Round up so the edge pixels get a group; the shader skips the overhang
				const auto& group = ctx.GetThreadGroup();
				ctx.Dispatch((width + group[0] - 1) / group[0], (height + group[1] - 1) / group[1]);
			});

		return output;
	}
}
