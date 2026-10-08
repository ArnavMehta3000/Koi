#include "Koi/Engine/Graphics/RenderPasses/DebugDrawPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <cassert>

namespace koi
{
	DebugDrawPass::DebugDrawPass(const Config& config)
		: m_config(config)
	{
		assert(m_config.Device && m_config.Queue && "DebugDrawPass requires a device and a queue");

		m_vertexBuffer.Init(*m_config.Device, BufferUsage::Storage, 1024, "DebugVertex");
	}

	SceneTargets DebugDrawPass::Record(RenderGraph& graph, const Camera& camera, SceneTargets targets)
	{
		ZoneScoped;
		if (!targets.Color || !m_config.Resources || !m_config.Resources->DebugLinePipeline) return targets;

		graph.AddPass<RasterPass>("Debug Draw",
			[&](RGBuilder& b)
			{
				targets.Color = b.ColorTarget(targets.Color);
				targets.Depth = b.DepthTarget(targets.Depth);
			},
			[this, &camera](RasterContext& ctx)
			{
				const std::span<const DebugVertex> vertices = m_config.Queue->GetVertices();
				if (vertices.empty()) return;

				const u32 frameSlot = static_cast<u32>(m_config.Device->GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);
				const DescriptorIndex vertexBufferIndex = m_vertexBuffer.Upload(frameSlot, vertices);
				if (vertexBufferIndex == INVALID_DESCRIPTOR) return;

				ctx.SetPipeline(m_config.Resources->DebugLinePipeline);
				ctx.SetRootConstants(DebugDrawRootConstants
				{
					.ViewProjection    = camera.ViewProjection,
					.VertexBufferIndex = vertexBufferIndex,
					._Pad              = {},
				});
				ctx.Draw(static_cast<u32>(vertices.size()));
			});

		return targets;
	}
}
