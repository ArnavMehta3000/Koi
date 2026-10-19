#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Content/Shaders/Interop/PostProcess.hlsli"
#include "Koi/Engine/Diagnostics/Profiling.h"

namespace koi
{
	namespace
	{
		constexpr ComputePipeline kTonemap
		{
			.ComputeShader = "engine://Content/Shaders/TonemapCS",
		};
	}

	void TonemapPass::Record(RenderGraph& graph, const RenderView&, World&)
	{
		ZoneScoped;
		RGBlackboard& bb = graph.GetBlackboard();

		const SceneColor* scene = bb.TryGet<SceneColor>();
		if (!scene) return;

		const RGTexture   hdr  = scene->Color;
		const TextureDesc desc = graph.GetDesc(hdr);
		RGTexture output = graph.CreateTexture(TextureDesc{ .Width = desc.Width, .Height = desc.Height, .Format = Format::R8G8B8A8_UNORM }, "LDRColor");

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

				const auto& group = ctx.GetThreadGroup();
				ctx.Dispatch((width + group[0] - 1) / group[0], (height + group[1] - 1) / group[1]);
			});

		bb.Add(DisplayColor{ .Color = output });
	}
}
