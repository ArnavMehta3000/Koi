#include "Koi/Engine/Graphics/RenderPasses/ScenePass.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	// Mirrors the HLSL b0 root-constant layout used by the default mesh shaders.
	struct MeshPushConstants
	{
		float4x4 MVP;
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
		m_world  = nullptr;
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
		if (!colorTarget || !m_world || !m_camera || !m_config.Resources) return;

		flecs::world_t* ecsWorld = m_world->GetECS().c_ptr();
		if (m_queryWorld != ecsWorld)
		{
			// `this` is captured (not per-frame data like the CommandList) since the ScenePass
			// itself outlives every frame; per-frame state comes in through it.param() below.
			m_renderSystem = m_world->GetECS().system<const WorldTransform, const MeshRef>("SceneMeshRenderSystem")
				.kind(0)  // never auto-scheduled by world.progress(); only invoked via run() below
				.each([this](flecs::iter& it, size_t, const WorldTransform& worldXform, const MeshRef& meshRef)
				{
					const GPUMesh* mesh = m_world->GetMesh(meshRef.Handle);
					if (!mesh) return;

					RenderContext* ctx = it.param<RenderContext>();

					MeshPushConstants pc{ .SamplerIndex = ctx->Resources->LinearWrapSampler };
					pc.MVP = mul(worldXform.Value, ctx->Camera->ViewProjection);
					pc.MVP = transpose(pc.MVP);
					pc.VertexBufferIndex = mesh->VertexBufferSRV;

					ctx->Cmd->SetIndexBuffer(mesh->IndexBuffer);

					for (const GPUMeshDraw& d : mesh->DrawItems)
					{
						pc.BaseColorIndex         = d.BaseColorSRV;
						// pc.MetallicRoughnessIndex = d.MetallicRoughnessSRV;
						// pc.NormalIndex            = d.NormalSRV;
						// pc.OcclusionIndex         = d.OcclusionSRV;
						// pc.EmissiveIndex          = d.EmissiveSRV;

						ctx->Cmd->SetPushConstants(&pc, sizeof(pc) / sizeof(u32));
						ctx->Cmd->DrawIndexed(d.IndexCount, 1, d.IndexOffset, d.VertexOffset);
					}
				});
			m_queryWorld = ecsWorld;
		}

		const PipelineHandle pipeline = m_config.Pipeline ? m_config.Pipeline : m_config.Resources->MeshOpaquePipeline;

		cmd.SetRenderTarget(colorTarget, depthTarget);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(pipeline);

		RenderContext ctx{ &cmd, m_camera, m_config.Resources, pipeline };
		m_renderSystem.run(0.0f, &ctx);
	}
}
