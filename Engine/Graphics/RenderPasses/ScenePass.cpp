#include "Graphics/RenderPasses/ScenePass.h"
#include "Graphics/RendererResources.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/Camera.h"
#include "Scene/Scene.h"
#include "Core/Math.h"

namespace koi
{
	// Mirrors the HLSL b0 root-constant layout used by the default mesh shaders.
	struct MeshPushConstants
	{
		math::float4x4 MVP;
		u32 VertexBufferIndex;
		DescriptorIndex BaseColorIndex;
		//DescriptorIndex MetallicRoughnessIndex;
		//DescriptorIndex NormalIndex;
		//DescriptorIndex OcclusionIndex;
		//DescriptorIndex EmissiveIndex;
		DescriptorIndex SamplerIndex;

		// Maybe the root constant is not big enough for all of these
	};

	ScenePass::ScenePass(const Desc& desc)
		: m_desc(desc)
	{
		m_outputs[0] = { m_desc.ColorTarget, "HDRColor" };
		m_outputs[1] = { m_desc.DepthBuffer, "Depth" };
	}

	void ScenePass::Execute(CommandList& cmd)
	{
		if (!m_desc.ColorTarget || !m_desc.Scene || !m_desc.Camera || !m_desc.Resources) return;

		const PipelineHandle pipeline = m_desc.Pipeline ? m_desc.Pipeline : m_desc.Resources->MeshOpaquePipeline;

		cmd.SetRenderTarget(m_desc.ColorTarget, m_desc.DepthBuffer);
		cmd.SetViewportAndScissor(m_desc.Width, m_desc.Height);
		cmd.SetPipeline(pipeline);

		MeshPushConstants pc{ .SamplerIndex = m_desc.Resources->LinearWrapSampler };

		for (const SceneNode& node : m_desc.Scene->GetNodes())
		{
			if (node.MeshIndex < 0) continue;
			const GPUMesh& mesh = m_desc.Scene->GetMesh(node.MeshIndex);

			pc.MVP = math::mul(node.WorldTransform, m_desc.Camera->ViewProjection);
			pc.MVP = math::transpose(pc.MVP);
			pc.VertexBufferIndex = mesh.VertexBufferSRV;

			cmd.SetIndexBuffer(mesh.IndexBuffer);

			for (const GPUMeshDraw& d : mesh.DrawItems)
			{
				pc.BaseColorIndex         = d.BaseColorSRV;
				// pc.MetallicRoughnessIndex = d.MetallicRoughnessSRV;
				// pc.NormalIndex            = d.NormalSRV;
				// pc.OcclusionIndex         = d.OcclusionSRV;
				// pc.EmissiveIndex          = d.EmissiveSRV;

				cmd.SetPushConstants(&pc, sizeof(pc) / sizeof(u32));
				cmd.DrawIndexed(d.IndexCount, 1, d.IndexOffset, d.VertexOffset);
			}
		}
	}
}
