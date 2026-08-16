#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <cassert>

namespace koi
{
	ShadowPass::ShadowPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		assert(config.Device && "ShadowPass requires a valid GfxDevice");

		m_frameConstants.Init(*m_config.Device, BufferUsage::Constant, 1,  "Shadow FrameConstants");
		m_objects.Init      (*m_config.Device, BufferUsage::Storage,  64, "Shadow PerObjectData");

		// Fixed size, so unlike the G-buffer this is created once and never resized
		m_output.Texture = m_config.Device->CreateTexture(TextureDesc
		{
			.Width     = m_config.Resolution,
			.Height    = m_config.Resolution,
			.Format    = Format::D32_FLOAT,
			.Usage     = TextureUsage::DepthStencil | TextureUsage::ShaderRead,
			.DebugName = "Shadow Map",
		});
	}

	ShadowPass::~ShadowPass()
	{
		ZoneScoped;
		if (!m_config.Device) return;

		m_config.Device->Destroy(m_output.Texture);
		m_output.Texture = TextureHandle();
	}

	ShadowPass& ShadowPass::SetWorld(World* world)
	{
		m_world     = world;
		m_hasShadow = ComputeLightMatrix();
		return *this;
	}

	bool ShadowPass::ComputeLightMatrix()
	{
		ZoneScoped;
		if (!m_world) return false;

		const AABB& bounds = m_world->GetBounds();
		if (bounds.Min.x > bounds.Max.x) return false;  // nothing loaded yet

		Vector3 lightDir;
		bool found = false;
		m_world->GetECS()
			.query<const Transform, const Light>()
			.each([&](const Transform& xform, const Light& light)
			{
				if (found || !light.CastsShadows || light.Type != LightType::Directional) return;

				lightDir = Matrix::CreateFromQuaternion(xform.Rotation).Forward();
				lightDir.Normalize();
				found = true;
			});

		if (!found) return false;

		// Fit the ortho box to the scene's bounding sphere rather than the camera frustum:
		// the map stays stable as the camera moves, at the cost of texel density
		// [NOTE]: Maybe I can move the ortho box around the camera once the world gets bigger
		const Vector3 center = bounds.Center();
		const f32     radius = bounds.Radius() > 0.001f ? bounds.Radius() : 0.001f;
		const Vector3 eye    = center - lightDir * (radius * 2.0f);

		// Any up vector works except one parallel to the light
		const Vector3 up = (lightDir.y > 0.99f || lightDir.y < -0.99f) ? Vector3::Right : Vector3::Up;

		const Matrix view = Matrix::CreateLookAt(eye, center, up);
		const Matrix proj = Matrix::CreateOrthographic(radius * 2.0f, radius * 2.0f, 0.0f, radius * 4.0f);

		m_lightViewProjection = view * proj;
		return true;
	}

	void ShadowPass::ConfigureECS()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("ShadowPass: (re)building mesh gather system for new world");

		m_renderSystem = m_world->GetECS()
			.system<const Transform, const MeshRef>("ShadowMeshGatherSystem")
			.kind(0)  // never auto-scheduled by world.progress() | only invoked via run() below
			.each([this](flecs::iter& it, size_t, const Transform& xform, const MeshRef& meshRef)
			{
				GatherContext* gather = it.param<GatherContext>();
				const GPUMesh* mesh   = m_world->GetMesh(meshRef.Handle);
				if (!mesh) return;

				// Depth-only, so the normal matrix is left identity
				const u32 objectIndex = static_cast<u32>(gather->Objects.size());
				gather->Objects.push_back(PerObjectData{ .World = xform.ToMatrix() });
				gather->DrawInfos.push_back(DrawInfo{ mesh, objectIndex });
			});
	}

	void ShadowPass::Execute(CommandList& cmd)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		const TextureHandle shadowMap = m_output.Texture;
		if (!m_hasShadow || !shadowMap || !m_world || !m_config.Resources) return;

		GfxDevice& device   = *m_config.Device;
		const u32  frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		if (flecs::world_t* ecsWorld = m_world->GetECS().c_ptr(); m_queryWorld != ecsWorld)
		{
			m_queryWorld = ecsWorld;
			ConfigureECS();
		}

		m_gather.Clear();
		m_renderSystem.run(0.0f, &m_gather);

		// Clear unconditionally: LightingPass samples the map whenever m_hasShadow, so an
		// empty draw list still has to leave a valid RT/Depth map behind
		cmd.ClearDepthStencil(shadowMap);

		if (!m_gather.DrawInfos.empty())
		{
			const DescriptorIndex objectBufferIndex = m_objects.Upload(frameSlot, std::span<const PerObjectData>(m_gather.Objects));

			const DescriptorIndex frameConstantsIndex = m_frameConstants.Upload(frameSlot, FrameConstants
			{
				.ViewProjection = m_lightViewProjection,
				.CameraPosition = Vector4::Zero,
			});

			// No color targets at all - the PSO declares RenderTargetCount = 0 to match
			cmd.SetRenderTarget(TextureHandle(), shadowMap);
			cmd.SetViewportAndScissor(m_config.Resolution, m_config.Resolution);
			cmd.SetPipeline(m_config.Resources->ShadowPipeline);

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
						.MaterialBufferIndex = INVALID_DESCRIPTOR,
						.MaterialIndex       = 0,
						.VertexBufferIndex   = mesh.VertexBufferSRV,
						.SamplerIndex        = INVALID_DESCRIPTOR,
						.VertexOffset        = static_cast<u32>(d.VertexOffset),
					};
					cmd.SetRootConstants(&pc, sizeof(pc) / sizeof(u32));
					cmd.DrawIndexed(d.IndexCount, 1, d.IndexOffset, 0);
				}
			}
		}

		cmd.TransitionBarrier(shadowMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.FlushBarriers();
	}

	void ShadowPass::Reset()
	{
		m_world     = nullptr;
		m_hasShadow = false;
	}
}
