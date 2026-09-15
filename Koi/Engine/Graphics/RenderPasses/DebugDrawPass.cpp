#include "Koi/Engine/Graphics/RenderPasses/DebugDrawPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
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

	DebugDrawPass::~DebugDrawPass() = default;

	void DebugDrawPass::Reset()
	{
		m_camera      = nullptr;
		m_colorTarget = {};
		m_depth       = {};
		m_width       = 0;
		m_height      = 0;
		m_lineCount   = 0;
	}

	DebugDrawPass& DebugDrawPass::SetTarget(TextureHandle colorTarget, TextureHandle depth, u32 width, u32 height)
	{
		m_colorTarget = colorTarget;
		m_depth       = depth;
		m_width       = width;
		m_height      = height;
		return *this;
	}

	void DebugDrawPass::Execute(CommandList& cmd)
	{
		ZoneScoped;
		if (!m_colorTarget || !m_camera || !m_config.Resources || !m_config.Resources->DebugLinePipeline) return;

		const std::span<const DebugVertex> vertices = m_config.Queue->GetVertices();
		if (vertices.empty()) return;

		GfxDevice& device   = *m_config.Device;
		const u32  frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		const DescriptorIndex vertexBufferIndex = m_vertexBuffer.Upload(frameSlot, vertices);
		if (vertexBufferIndex == INVALID_DESCRIPTOR) return;

		cmd.TransitionBarrier(m_colorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// The lighting pass left the depth buffer readable as an SRV; a writable DSV needs DEPTH_WRITE
		// even though the pipeline only tests against it
		if (m_depth)
			cmd.TransitionBarrier(m_depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		cmd.SetRenderTarget(m_colorTarget, m_depth);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(m_config.Resources->DebugLinePipeline);

		const DebugDrawRootConstants pc
		{
			.ViewProjection    = m_camera->ViewProjection,
			.VertexBufferIndex = vertexBufferIndex,
			._Pad              = {},
		};
		cmd.SetRootConstants(&pc, sizeof(pc) / sizeof(u32));

		m_lineCount = static_cast<u32>(vertices.size()) / 2;
		cmd.Draw(static_cast<u32>(vertices.size()));
	}
}
