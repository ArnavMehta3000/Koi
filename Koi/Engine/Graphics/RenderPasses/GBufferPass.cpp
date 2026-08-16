#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
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
	GBufferPass::GBufferPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		assert(config.Device && "GBufferPass requires a valid GfxDevice");

		m_frameConstants.Init(*m_config.Device, BufferUsage::Constant, 1,  "GBuffer FrameConstants");
		m_objects.Init      (*m_config.Device, BufferUsage::Storage,  64, "GBuffer PerObjectData");
	}

	GBufferPass::~GBufferPass()
	{
		ZoneScoped;
		if (!m_config.Device) return;

		DestroyRenderTargets();
	}

	void GBufferPass::Execute(CommandList& cmd)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		const TextureHandle albedo   = m_outputs[size_t(Outputs::Albedo)].Texture;
		const TextureHandle normals  = m_outputs[size_t(Outputs::Normals)].Texture;
		const TextureHandle material = m_outputs[size_t(Outputs::Material)].Texture;
		const TextureHandle depth    = m_outputs[size_t(Outputs::Depth)].Texture;
		if (!albedo || !m_world || !m_camera || !m_config.Resources) return;

		GfxDevice& device   = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		if (flecs::world_t* ecsWorld = m_world->GetECS().c_ptr(); m_queryWorld != ecsWorld)
		{
			m_queryWorld = ecsWorld;
			ConfigureECS();
		}

		// Gather this frame's per-object data + draw list
		m_gather.Clear();
		m_renderSystem.run(0.0f, &m_gather);
		if (m_gather.DrawInfos.empty()) return;

		const DescriptorIndex objectBufferIndex = m_objects.Upload(frameSlot, std::span<const PerObjectData>(m_gather.Objects));

		const DescriptorIndex frameConstantsIndex = m_frameConstants.Upload(frameSlot, FrameConstants
		{
			.ViewProjection = m_camera->ViewProjection,
			.CameraPosition = Vector4(m_camera->Position.x, m_camera->Position.y, m_camera->Position.z, 0.0f),
		});

		constexpr f32 kClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd.ClearRenderTarget(albedo,   kClearColor);
		cmd.ClearRenderTarget(normals,  kClearColor);
		cmd.ClearRenderTarget(material, kClearColor);
		if (depth) cmd.ClearDepthStencil(depth);

		// Bind
		std::array colorRTs{ albedo, normals, material };
		cmd.SetRenderTarget(colorRTs, depth);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(m_config.Resources->GBufferPipeline);

		// Issue draws
		for (const DrawInfo& info : m_gather.DrawInfos)
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
				cmd.SetRootConstants(&pc, sizeof(pc) / sizeof(u32));
				cmd.DrawIndexed(d.IndexCount, 1, d.IndexOffset, 0);
			}
		}

		// Leave all outputs shader-readable
		cmd.TransitionBarrier(albedo,   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.TransitionBarrier(normals,  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.TransitionBarrier(material, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		if (depth) cmd.TransitionBarrier(depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.FlushBarriers();
	}

	void GBufferPass::Reset()
	{
		// Clear only transient per-frame inputs
		m_world  = nullptr;
		m_camera = nullptr;
	}

	GBufferPass& GBufferPass::SetTarget(u32 width, u32 height)
	{
		ZoneScoped;
		if (width == m_width && height == m_height) return *this;

		m_width  = width;
		m_height = height;

		// Release the old targets and recreate at the new size
		DestroyRenderTargets();
		CreateRenderTargets();
		return *this;
	}

	void GBufferPass::DestroyRenderTargets()
	{
		for (RenderPassAttachment& o : m_outputs)
		{
			if (o.Texture) m_config.Device->Destroy(o.Texture);
			o.Texture = TextureHandle();
		}
	}

	void GBufferPass::ConfigureECS()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("GBufferPass: (re)building mesh gather system for new world");

		m_renderSystem = m_world->GetECS().system<const Transform, const MeshRef>("GBufferMeshGatherSystem")
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

	void GBufferPass::CreateRenderTargets()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("GBufferPass: creating render targets at {}x{}", m_width, m_height);

		TextureDesc baseTexDesc
		{
			.Width  = m_width,
			.Height = m_height,
			.Format = Format::R8G8B8A8_UNORM,
			.Usage  = TextureUsage::RenderTarget | TextureUsage::ShaderRead
		};

		baseTexDesc.DebugName   = "GBuffer Albedo";
		TextureHandle albedoSRV = m_config.Device->CreateTexture(baseTexDesc);

		baseTexDesc.Format      = Format::R16G16B16A16_UNORM;
		baseTexDesc.DebugName   = "GBuffer Normal";
		TextureHandle normalSRV = m_config.Device->CreateTexture(baseTexDesc);

		baseTexDesc.Format        = Format::R8G8B8A8_UNORM;
		baseTexDesc.DebugName     = "GBuffer Material";
		TextureHandle materialSRV = m_config.Device->CreateTexture(baseTexDesc);

		// Depth: sampleable D32. DepthStencil | ShaderRead makes the device back it with a
		// typeless resource carrying both a D32 DSV (to render into) and an R32 SRV (to sample)
		TextureHandle depth = m_config.Device->CreateTexture(TextureDesc
		{
			.Width     = m_width,
			.Height    = m_height,
			.Format    = Format::D32_FLOAT,
			.Usage     = TextureUsage::DepthStencil | TextureUsage::ShaderRead,
			.DebugName = "GBuffer Depth",
		});

		m_outputs[size_t(Outputs::Albedo)]   = { albedoSRV  , "Albedo"   };
		m_outputs[size_t(Outputs::Normals)]  = { normalSRV  , "Normals"  };
		m_outputs[size_t(Outputs::Material)] = { materialSRV, "Material" };
		m_outputs[size_t(Outputs::Depth)]    = { depth      , "Depth"    };
	}
}
