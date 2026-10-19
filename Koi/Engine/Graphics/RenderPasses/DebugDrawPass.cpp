#include "Koi/Engine/Graphics/RenderPasses/DebugDrawPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <cassert>

namespace koi
{
	namespace
	{
		constexpr RasterPipeline kDebugLines
		{
			.VertexShader = "engine://Content/Shaders/DebugLineVS",
			.PixelShader  = "engine://Content/Shaders/DebugLinePS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.DepthStencil = DepthStencilDesc::Read(),
			.Topology     = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
		};
	}

	DebugDrawPass::DebugDrawPass(const Config& config)
		: m_config(config)
	{
		assert(m_config.Device && m_config.Queue && "DebugDrawPass requires a device and a queue");

		m_vertexBuffer.Init(*m_config.Device, BufferUsage::Storage, 1024, "DebugVertex");
	}

	void DebugDrawPass::Record(RenderGraph& graph, const RenderView& view, World&)
	{
		ZoneScoped;
		const Camera& camera = *view.Camera;
		RGBlackboard& bb = graph.GetBlackboard();

		DisplayColor* color = bb.TryGet<DisplayColor>();
		SceneDepth*   depth = bb.TryGet<SceneDepth>();
		if (!color) return;

		graph.AddPass<RasterPass>("Debug Draw",
			[&](RGBuilder& b)
			{
				color->Color = b.ColorTarget(color->Color);
				if (depth)
				{
					depth->Depth = b.DepthTarget(depth->Depth);
				}
			},
			[this, &camera](RasterContext& ctx)
			{
				const std::span<const DebugVertex> vertices = m_config.Queue->GetVertices();
				if (vertices.empty()) return;

				const u32 frameSlot = static_cast<u32>(m_config.Device->GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);
				const DescriptorIndex vertexBufferIndex = m_vertexBuffer.Upload(frameSlot, vertices);
				if (vertexBufferIndex == INVALID_DESCRIPTOR || !ctx.SetPipeline(kDebugLines)) return;

				ctx.SetRootConstants(DebugDrawRootConstants
				{
					.ViewProjection    = camera.ViewProjection,
					.VertexBufferIndex = vertexBufferIndex,
				});
				ctx.Draw(static_cast<u32>(vertices.size()));
			});
	}
}
