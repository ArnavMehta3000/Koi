#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Math/Math.h"
#include <cassert>
#include <vector>

namespace koi
{
	LightingPass::LightingPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		assert(m_config.Device && "LightingPass requires a valid GfxDevice");

		m_lightBuffer.Init(*m_config.Device, BufferUsage::Storage, 16, "GPULight");

		// Input names must match GBufferPass output names for graph connectivity validation.
		m_gbuffer[size_t(GBuf::Albedo)]   = { {}, "Albedo"   };
		m_gbuffer[size_t(GBuf::Normals)]  = { {}, "Normals"  };
		m_gbuffer[size_t(GBuf::Material)] = { {}, "Material" };
		m_gbuffer[size_t(GBuf::Depth)]    = { {}, "Depth"    };
	}

	LightingPass::~LightingPass() = default;

	void LightingPass::Reset()
	{
		m_world          = nullptr;
		m_camera         = nullptr;
		m_colorTarget    = {};
		m_output.Texture = {};
		m_width          = 0;
		m_height         = 0;
		for (RenderPassAttachment& g : m_gbuffer)
			g.Texture = {};
	}

	LightingPass& LightingPass::SetGBuffer(TextureHandle albedo, TextureHandle normals, TextureHandle material, TextureHandle depth)
	{
		m_gbuffer[size_t(GBuf::Albedo)].Texture   = albedo;
		m_gbuffer[size_t(GBuf::Normals)].Texture  = normals;
		m_gbuffer[size_t(GBuf::Material)].Texture = material;
		m_gbuffer[size_t(GBuf::Depth)].Texture    = depth;
		return *this;
	}

	LightingPass& LightingPass::SetTarget(TextureHandle colorTarget, u32 width, u32 height)
	{
		m_colorTarget    = colorTarget;
		m_output.Texture = colorTarget;
		m_width          = width;
		m_height         = height;
		return *this;
	}

	void LightingPass::ConfigureECS()
	{
		ZoneScoped;
		m_gatherSystem = m_world->GetECS().system<const Transform, const Light>("LightGatherSystem")
			.kind(0)  // never auto-scheduled by world.progress() | only invoked via run() below
			.each([](flecs::iter& it, size_t, const Transform& xform, const Light& light)
			{
				std::vector<GPULight>* lights = it.param<std::vector<GPULight>>();

				// Directional lights use the entity's forward; point lights use its position.
				Vector3 forward = Matrix::CreateFromQuaternion(xform.Rotation).Forward();
				forward.Normalize();

				lights->push_back(GPULight
				{
					.Position  = xform.Position,
					.Type      = static_cast<u32>(light.Type),
					.Direction = forward,
					.Range     = light.Range,
					.Color     = light.Color,
					.Intensity = light.Intensity,
				});
			});
	}

	void LightingPass::Execute(CommandList& cmd)
	{
		ZoneScoped;
		if (!m_world) return;

		GfxDevice& device = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		// Rebuild the gather system if the world was first encountered or changed.
		if (flecs::world_t* ecsWorld = m_world->GetECS().c_ptr(); m_queryWorld != ecsWorld)
		{
			m_queryWorld = ecsWorld;
			ConfigureECS();
		}

		std::vector<GPULight> lights;
		m_gatherSystem.run(0.0f, &lights);

		m_lightCount = static_cast<u32>(lights.size());
		if (lights.empty()) return;

		const DescriptorIndex lightBufferIndex = m_lightBuffer.Upload(frameSlot, std::span<const GPULight>(lights));

		// --- Full-screen lighting draw ---
		if (!m_colorTarget || !m_camera || !m_config.Resources || !m_config.Resources->LightingPipeline)
			return;

		const Texture* albedoTex   = device.Get(m_gbuffer[size_t(GBuf::Albedo)].Texture);
		const Texture* normalTex   = device.Get(m_gbuffer[size_t(GBuf::Normals)].Texture);
		const Texture* materialTex = device.Get(m_gbuffer[size_t(GBuf::Material)].Texture);
		const Texture* depthTex    = device.Get(m_gbuffer[size_t(GBuf::Depth)].Texture);
		if (!albedoTex || !normalTex || !materialTex || !depthTex) return;

		// G-buffer outputs were left shader-readable by GBufferPass; only the HDR target needs transitioning.
		cmd.TransitionBarrier(m_colorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd.FlushBarriers();

		cmd.SetRenderTarget(m_colorTarget);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(m_config.Resources->LightingPipeline);

		LightingRootConstants pc
		{
			.InvViewProjection = m_camera->ViewProjection.Invert(),
			.CameraPosition    = Vector4(m_camera->Position.x, m_camera->Position.y, m_camera->Position.z, 0.0f),
			.AlbedoIndex       = albedoTex->SRVIndex,
			.NormalIndex       = normalTex->SRVIndex,
			.MaterialIndex     = materialTex->SRVIndex,
			.DepthIndex        = depthTex->SRVIndex,
			.LightBufferIndex  = lightBufferIndex,
			.LightCount        = m_lightCount,
			.SamplerIndex      = m_config.Resources->PointClampSampler,
			._Pad              = 0,
		};
		cmd.SetRootConstants(&pc, sizeof(pc) / sizeof(u32));
		cmd.Draw(3);
	}
}
