module;
#include "Graphics/RHI/DX12Common.h"
#include "Core/Config.h"
#include <array>
#include <hlsl++.h>
module Koi.Graphics.RenderPasses.ScenePass;

import Koi.Core.Types;
import Koi.Core.Math;
import Koi.Graphics.Types;
import Koi.Graphics.GPUMesh;
import Koi.Graphics.Camera;
import Koi.Graphics.RHI.CommandList;
import Koi.Graphics.RenderWindow;
import Koi.Graphics.RendererResources;
import Koi.Graphics.RenderPass;
import Koi.Scene;

namespace koi
{
	struct MeshPushConstants
	{
		f32 MVP[16];
		u32 VertexBufferIndex;
		u32 BaseColorTextureIndex;
		u32 SamplerIndex;
	};

	ScenePass::ScenePass(const Desc& desc)
		: m_desc(desc)
	{
		RenderWindow* rw = m_desc.RenderWindow;
		m_outputs[0] = { rw ? rw->GetHDRColorBuffer() : TextureHandle{}, "HDRColor" };
		m_outputs[1] = { m_desc.DepthBuffer, "Depth" };
	}

	std::span<const RenderPassAttachment> ScenePass::GetOutputs() const
	{
		return m_outputs;
	}

	void ScenePass::Execute(CommandList& cmd)
	{
		RenderWindow* rw = m_desc.RenderWindow;
		if (!rw || !m_desc.Scene || !m_desc.Camera || !m_desc.Resources) return;

		const PipelineHandle pipeline = m_desc.Pipeline ? m_desc.Pipeline : m_desc.Resources->MeshOpaquePipeline;

		cmd.SetRenderTarget(rw->GetHDRColorBuffer(), m_desc.DepthBuffer);
		cmd.SetViewportAndScissor(rw->GetWidth(), rw->GetHeight());
		cmd.SetPipeline(pipeline);

		MeshPushConstants pc{};
		pc.SamplerIndex = m_desc.Resources->LinearWrapSampler;

		for (const SceneNode& node : m_desc.Scene->GetNodes())
		{
			if (node.MeshIndex < 0) continue;
			const GPUMesh& mesh = m_desc.Scene->GetMesh(node.MeshIndex);

			math::store(pc.MVP, mul(node.WorldTransform, m_desc.Camera->ViewProjection));
			pc.VertexBufferIndex = mesh.VertexBufferSRV;

			cmd.SetIndexBuffer(mesh.IndexBuffer);

			for (const GPUMeshDraw& d : mesh.DrawItems)
			{
				pc.BaseColorTextureIndex = d.BaseColorSRV;
				cmd.SetPushConstants(&pc, sizeof(pc) / sizeof(u32));
				cmd.DrawIndexed(d.IndexCount, 1, d.IndexOffset, d.VertexOffset);
			}
		}
	}
}
