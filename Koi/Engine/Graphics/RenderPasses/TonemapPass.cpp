#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Content/Shaders/Interop/PostProcess.hlsli"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/RenderScene.h"

namespace koi
{
	namespace
	{
		constexpr ComputePipeline kTonemap
		{
			.ComputeShader = "engine://Content/Shaders/TonemapCS",
		};

		static_assert(static_cast<u32>(Tonemapper::ACES)       == TONEMAPPER_ACES);
		static_assert(static_cast<u32>(Tonemapper::AgX)        == TONEMAPPER_AGX);
		static_assert(static_cast<u32>(Tonemapper::PBRNeutral) == TONEMAPPER_PBR_NEUTRAL);
		static_assert(static_cast<u32>(Tonemapper::Hable)      == TONEMAPPER_HABLE);
		static_assert(static_cast<u32>(Tonemapper::Reinhard)   == TONEMAPPER_REINHARD);
		static_assert(static_cast<u32>(Tonemapper::None)       == TONEMAPPER_NONE);
	}

	void TonemapPass::Record(RenderGraph& graph, const RenderView&, const RenderScene& renderScene)
	{
		ZoneScoped;
		RGBlackboard& bb = graph.GetBlackboard();

		const SceneColor* scene = bb.TryGet<SceneColor>();
		if (!scene) return;

		const PostProcess& post = renderScene.GetPostProcess();
		const u32 op = static_cast<u32>(post.Tonemap.Operator);

		const RGTexture   hdr  = scene->Color;
		const TextureDesc desc = graph.GetDesc(hdr);
		RGTexture output = graph.CreateTexture(TextureDesc{ .Width = desc.Width, .Height = desc.Height, .Format = Format::R8G8B8A8_UNORM }, "LDRColor");

		const ExposureData* exposureData = bb.TryGet<ExposureData>();
		const RGBuffer      exposure     = exposureData ? exposureData->Exposure : RGBuffer();

		graph.AddPass<ComputePass>("Tonemap",
			[&](RGBuilder& b)
			{
				b.Read(hdr);
				b.Read(exposure);
				output = b.Write(output);
			},
			[hdr, exposure, op, ldr = output, width = desc.Width, height = desc.Height](ComputeContext& ctx)
			{
				if (!ctx.SetPipeline(kTonemap)) return;
				ctx.SetRootConstants(TonemapRootConstants
				{
					.HDRTextureIndex = ctx.SRV(hdr),
					.LDRTextureIndex = ctx.UAV(ldr),
					.ExposureIndex   = exposure ? ctx.SRV(exposure) : INVALID_DESCRIPTOR,
					.Operator        = op,
				});

				const auto& group = ctx.GetThreadGroup();
				ctx.Dispatch((width + group[0] - 1) / group[0], (height + group[1] - 1) / group[1]);
			});

		bb.Add(DisplayColor{ .Color = output });
	}
}
