#include "Koi/Engine/Graphics/RenderPasses/ScenePass.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Math/Math.h"
#include <cassert>
#include <cstring>

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
		assert(m_config.Device && "ScenePass requires a valid GfxDevice");

		m_outputs[0] = { {}, "HDRColor" };
		m_outputs[1] = { {}, "Depth" };

		// Create buffers for frame constants.
		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			m_frameConstantsBuffers[i] = m_config.Device->CreateBuffer(BufferDesc
			{
				.Size      = sizeof(FrameConstants),
				.Usage     = BufferUsage::Constant,
				.Memory    = MemoryUsage::CpuToGpu,
				.DebugName = "FrameConstants",
			});
		}
	}

	ScenePass::~ScenePass()
	{
		if (!m_config.Device) return;

		for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			m_config.Device->Destroy(m_frameConstantsBuffers[i]);
			if (m_objectBuffers[i])
				m_config.Device->Destroy(m_objectBuffers[i]);
		}
	}

	void ScenePass::EnsureObjectCapacity(u32 frameSlot, u32 requiredCount)
	{
		if (requiredCount <= m_objectCapacity[frameSlot]) return;

		const u32 newCapacity = std::max(requiredCount, std::max(64u, m_objectCapacity[frameSlot] * 2));

		if (m_objectBuffers[frameSlot])
			m_config.Device->Destroy(m_objectBuffers[frameSlot]);

		m_objectBuffers[frameSlot] = m_config.Device->CreateBuffer(BufferDesc
		{
			.Size      = newCapacity * sizeof(PerObjectData),
			.Stride    = sizeof(PerObjectData),
			.Usage     = BufferUsage::Storage,
			.Memory    = MemoryUsage::CpuToGpu,
			.DebugName = "PerObjectData",
		});
		m_objectCapacity[frameSlot] = newCapacity;
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

		GfxDevice& device = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		// Set up scene pass rendering ECS system for this world (if world is first encountered or changed)
		flecs::world_t* ecsWorld = m_world->GetECS().c_ptr();
		if (m_queryWorld != ecsWorld)
		{
			m_renderSystem = m_world->GetECS().system<const WorldTransform, const MeshRef>("SceneMeshGatherSystem")
				.kind(0)  // never auto-scheduled by world.progress() | only invoked via run() below
				.each([this](flecs::iter& it, size_t, const WorldTransform& worldXform, const MeshRef& meshRef)
				{
					GatherContext* gather = it.param<GatherContext>();
					const GPUMesh* mesh = m_world->GetMesh(meshRef.Handle);
					if (!mesh) return;

					const u32 objectIndex = static_cast<u32>(gather->Objects.size());
					gather->Objects.push_back(PerObjectData
					{
						.World        = worldXform.Value,
						.NormalMatrix = worldXform.Value.Invert().Transpose(),
					});
					gather->DrawInfos.push_back(DrawInfo{ mesh, objectIndex });
				});
			m_queryWorld = ecsWorld;
		}


		// Gather this frame's per-object data + draw list
		GatherContext gather;
		m_renderSystem.run(0.0f, &gather);
		if (gather.DrawInfos.empty()) return;

		EnsureObjectCapacity(frameSlot, static_cast<u32>(gather.Objects.size()));

		// Copy per-object data to object buffer
		Buffer* objectBuf = device.Get(m_objectBuffers[frameSlot]);
		assert(objectBuf && objectBuf->Resource && objectBuf->MappedPtr && "Invalid object buffer!");
		std::memcpy(objectBuf->MappedPtr, gather.Objects.data(), gather.Objects.size() * sizeof(PerObjectData));

		// Copy frame constants
		Buffer* frameConstantsBuf = device.Get(m_frameConstantsBuffers[frameSlot]);
		assert(frameConstantsBuf && frameConstantsBuf->Resource && frameConstantsBuf->MappedPtr && "Invalid frame constants buffer!");

		const FrameConstants frameConstants
		{
			.ViewProjection = m_camera->ViewProjection,
			.CameraPosition = Vector4(m_camera->Position.x, m_camera->Position.y, m_camera->Position.z, 0.0f),
		};
		std::memcpy(frameConstantsBuf->MappedPtr, &frameConstants, sizeof(frameConstants));

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
					.FrameConstantsIndex = frameConstantsBuf->CBVIndex,
					.ObjectBufferIndex   = objectBuf->SRVIndex,
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
