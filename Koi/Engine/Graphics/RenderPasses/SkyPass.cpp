#include "Koi/Engine/Graphics/RenderPasses/SkyPass.h"
#include "Koi/Engine/Content/Shaders/Interop/Sky.hlsli"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/GPUTexture.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RenderScene.h"
#include "Koi/Engine/Resources/AssetRegistry.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	namespace
	{
		// SkyVS sits exactly on the far plane, so LESS_EQUAL passes only where depth is still cleared
		constexpr RasterPipeline kSky
		{
			.VertexShader = "engine://Content/Shaders/SkyVS",
			.PixelShader  = "engine://Content/Shaders/SkyPS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.Blend        = BlendTargetDesc::Opaque(),
			.DepthStencil = DepthStencilDesc::Read(),
		};
	}

	void SkyPass::Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene)
	{
		ZoneScoped;
		RGBlackboard& bb = graph.GetBlackboard();

		SceneColor* color = bb.TryGet<SceneColor>();
		SceneDepth* depth = bb.TryGet<SceneDepth>();
		if (!scene.GetSky() || !color || !depth || !m_config.Resources) return;

		const RenderSky sky    = *scene.GetSky();
		const Camera&   camera = *view.Camera;

		graph.AddPass<RasterPass>("Sky",
			[&](RGBuilder& b)
			{
				color->Color = b.ColorTarget(color->Color);
				depth->Depth = b.DepthTarget(depth->Depth);
			},
			[this, sky, &camera](RasterContext& ctx)
			{
				const GPUTexture* image = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>().Get(sky.Image);
				if (!ctx.SetPipeline(kSky)) return;

				ctx.SetRootConstants(SkyRootConstants
				{
					.InvViewProjection = camera.ViewProjection.Invert(),
					.CameraPosition    = camera.Position,
					.Rotation          = sky.Data.Rotation,
					.Radiance          = sky.Data.Color * sky.Data.Intensity,
					.ImageIndex        = image ? image->SRVIndex : m_config.Resources->WhiteTextureSRV,
					.SamplerIndex      = m_config.Resources->LatLongSampler,
				});
				ctx.Draw(3);
			});
	}
}
