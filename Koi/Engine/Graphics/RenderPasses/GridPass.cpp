#include "Koi/Engine/Graphics/RenderPasses/GridPass.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"

namespace koi
{
	namespace
	{
		// The pixel shader writes SV_Depth, so it depth-tests without writing
		constexpr RasterPipeline kGrid
		{
			.VertexShader = "engine://Content/Shaders/GridVS",
			.PixelShader  = "engine://Content/Shaders/GridPS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.Blend        = BlendTargetDesc::AlphaBlend(),
			.DepthStencil = DepthStencilDesc::Read(),
		};
	}

	GridPass::GridPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		KOI_ASSERT(m_config.Device, "GridPass requires a valid GfxDevice");

		m_constants.Init(*m_config.Device, BufferUsage::Constant, 1, "Grid Constants");
	}

	void GridPass::Record(RenderGraph& graph, const RenderView& view, World&)
	{
		ZoneScoped;
		const Camera& camera = *view.Camera;
		RGBlackboard& bb = graph.GetBlackboard();

		DisplayColor* color = bb.TryGet<DisplayColor>();
		SceneDepth*   depth = bb.TryGet<SceneDepth>();
		if (!m_settings.Enabled || !color || !depth) return;

		graph.AddPass<RasterPass>("Grid",
			[&](RGBuilder& b)
			{
				color->Color = b.ColorTarget(color->Color);
				depth->Depth = b.DepthTarget(depth->Depth);
			},
			[this, &camera](RasterContext& ctx)
			{
				const u32 frameSlot = static_cast<u32>(m_config.Device->GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

				const DescriptorIndex constantsIndex = m_constants.Upload(frameSlot, GridConstants
				{
					.ViewProjection    = camera.ViewProjection,
					.InvViewProjection = camera.ViewProjection.Invert(),
					.CameraPosition    = Vector4(camera.Position.x, camera.Position.y, camera.Position.z, 0.0f),
					.ThinColor         = m_settings.ThinColor,
					.ThickColor        = m_settings.ThickColor,
					.AxisXColor        = m_settings.AxisXColor,
					.AxisZColor        = m_settings.AxisZColor,
					.CellSize          = m_settings.CellSize,
					.ThickEvery        = m_settings.ThickEvery,
					.FadeStart         = m_settings.FadeStart,
					.FadeEnd           = m_settings.FadeEnd,
					.MajorLineWidth    = m_settings.MajorLineWidth,
				});
				if (constantsIndex == INVALID_DESCRIPTOR || !ctx.SetPipeline(kGrid)) return;

				ctx.SetRootConstants(GridRootConstants{ .GridConstantsIndex = constantsIndex });
				ctx.Draw(3);
			});
	}
}
