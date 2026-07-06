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

	ScenePass::ScenePass(const Config& config)
		: m_config(config)
	{
		m_outputs[0] = { {}, "HDRColor" };
		m_outputs[1] = { {}, "Depth" };
	}

	void ScenePass::Reset()
	{
		m_scene  = nullptr;
		m_camera = nullptr;
		m_width  = 0;
		m_height = 0;
		m_outputs[0].Texture = {};
		m_outputs[1].Texture = {};
	}

	ScenePass& ScenePass::SetTarget(TextureHandle colorTarget, TextureHandle depthTarget, u32 width, u32 height)
	{
		m_outputs[0].Texture = colorTarget;
		m_outputs[1].Texture = depthTarget;
		m_width              = width;
		m_height             = height;
		return *this;
	}

	void ScenePass::Execute(CommandList& cmd)
	{
		const TextureHandle colorTarget = m_outputs[0].Texture;
		const TextureHandle depthTarget = m_outputs[1].Texture;
		if (!colorTarget || !m_scene || !m_camera || !m_config.Resources) return;

		const PipelineHandle pipeline = m_config.Pipeline ? m_config.Pipeline : m_config.Resources->MeshOpaquePipeline;

		cmd.SetRenderTarget(colorTarget, depthTarget);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(pipeline);

		MeshPushConstants pc{ .SamplerIndex = m_config.Resources->LinearWrapSampler };

		for (const SceneNode& node : m_scene->GetNodes())
		{
			if (node.MeshIndex < 0) continue;
			const GPUMesh& mesh = m_scene->GetMesh(node.MeshIndex);

			pc.MVP = math::mul(node.WorldTransform, m_camera->ViewProjection);
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
