#include "Koi/Engine/Graphics/RenderPasses/ScenePass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <cassert>

namespace koi
{
	namespace
	{
		// Gathered once per Execute() by the flecs system below, then drained to issue draws.
		struct DrawInfo
		{
			const GPUMesh* Mesh;
			u32            ObjectIndex;
		};

		struct GatherContext
		{
			std::vector<PerObjectData> Objects;  // World + Normal Matrix
			std::vector<DrawInfo>      DrawInfos;
		};
	}

	ScenePass::ScenePass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		assert(m_config.Device && "ScenePass requires a valid GfxDevice");

		m_outputs[0] = { {}, "HDRColor" };
		m_outputs[1] = { {}, "Depth" };

		m_frameConstants.Init(*m_config.Device, BufferUsage::Constant, 1,  "FrameConstants");
		m_objects.Init      (*m_config.Device, BufferUsage::Storage,  64, "PerObjectData");
	}

	ScenePass::~ScenePass() = default;

	void ScenePass::ConfigureECS()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("ScenePass: (re)building mesh gather system for new world");

		m_renderSystem = m_world->GetECS().system<const Transform, const MeshRef>("SceneMeshGatherSystem")
			.kind(0)  // never auto-scheduled by world.progress() | only invoked via run() below
			.each([this](flecs::iter& it, size_t, const Transform& xform, const MeshRef& meshRef)
			{
				GatherContext* gather = it.param<GatherContext>();
				const GPUMesh* mesh   = m_world->GetMesh(meshRef.Handle);
				if (!mesh) return;

				const Matrix world = xform.ToMatrix();
				const u32 objectIndex = static_cast<u32>(gather->Objects.size());
				gather->Objects.push_back(PerObjectData
				{
					.World        = world,
					.NormalMatrix = world.Invert().Transpose(),
				});
				gather->DrawInfos.push_back(DrawInfo{ mesh, objectIndex });
			});
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
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		const TextureHandle colorTarget = m_outputs[0].Texture;
		const TextureHandle depthTarget = m_outputs[1].Texture;
		if (!colorTarget || !m_world || !m_camera || !m_config.Resources) return;

		GfxDevice& device = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		// Set up scene pass rendering ECS system for this world (if world is first encountered or changed)
		if (flecs::world_t* ecsWorld = m_world->GetECS().c_ptr(); m_queryWorld != ecsWorld)
		{
			m_queryWorld = ecsWorld;
			ConfigureECS();
		}

		// Gather this frame's per-object data + draw list
		GatherContext gather;
		m_renderSystem.run(0.0f, &gather);
		if (gather.DrawInfos.empty()) return;

		const DescriptorIndex objectBufferIndex = m_objects.Upload(frameSlot, std::span<const PerObjectData>(gather.Objects));

		const DescriptorIndex frameConstantsIndex = m_frameConstants.Upload(frameSlot, FrameConstants
		{
			.ViewProjection = m_camera->ViewProjection,
			.CameraPosition = Vector4(m_camera->Position.x, m_camera->Position.y, m_camera->Position.z, 0.0f),
		});

		const PipelineHandle pipeline = m_config.Pipeline ? m_config.Pipeline : m_config.Resources->MeshOpaquePipeline;

		cmd.SetRenderTarget(colorTarget, depthTarget);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(pipeline);

		// Issue draws
		for (const DrawInfo& info : gather.DrawInfos)
		{
			const GPUMesh& mesh = *info.Mesh;
			cmd.SetIndexBuffer(mesh.IndexBuffer);

			for (const GPUMeshDraw& d : mesh.DrawItems)
			{
				DrawRootConstants pc
				{
					.FrameConstantsIndex = frameConstantsIndex,
					.ObjectBufferIndex   = objectBufferIndex,
					.ObjectIndex         = info.ObjectIndex,
					.MaterialBufferIndex = mesh.MaterialBufferSRV,
					.MaterialIndex       = d.MaterialIndex,
					.VertexBufferIndex   = mesh.VertexBufferSRV,
					.SamplerIndex        = m_config.Resources->LinearWrapSampler,
					.VertexOffset        = static_cast<u32>(d.VertexOffset),
				};

				// Vertex fetch is fully bindless (no IA vertex buffer), so the per-primitive
				// vertex offset is applied in the shader via VertexOffset above, not through
				// BaseVertexLocation (whose effect on SV_VertexID is not something to rely on)
				cmd.SetRootConstants(&pc, sizeof(pc) / sizeof(u32));
				cmd.DrawIndexed(d.IndexCount, 1, d.IndexOffset, 0);
			}
		}
	}
}
