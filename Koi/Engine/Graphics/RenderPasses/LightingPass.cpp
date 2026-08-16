#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
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
	LightingPass::LightingPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		assert(m_config.Device && "LightingPass requires a valid GfxDevice");

		m_lightBuffer.Init(*m_config.Device, BufferUsage::Storage, 16, "GPULight");

		m_inputs[size_t(Input::Albedo)]   = { {}, "Albedo"    };
		m_inputs[size_t(Input::Normals)]  = { {}, "Normals"   };
		m_inputs[size_t(Input::Material)] = { {}, "Material"  };
		m_inputs[size_t(Input::Depth)]    = { {}, "Depth"     };
		m_inputs[size_t(Input::Shadow)]   = { {}, "ShadowMap" };
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

		for (RenderPassAttachment& in : m_inputs)
		{
			in.Texture = {};
		}
	}

	LightingPass& LightingPass::SetGBuffer(TextureHandle albedo, TextureHandle normals, TextureHandle material, TextureHandle depth)
	{
		m_inputs[size_t(Input::Albedo)].Texture   = albedo;
		m_inputs[size_t(Input::Normals)].Texture  = normals;
		m_inputs[size_t(Input::Material)].Texture = material;
		m_inputs[size_t(Input::Depth)].Texture    = depth;
		return *this;
	}

	LightingPass& LightingPass::SetShadow(TextureHandle shadowMap, const Matrix& lightViewProjection)
	{
		m_inputs[size_t(Input::Shadow)].Texture = shadowMap;
		m_lightViewProjection                   = lightViewProjection;
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
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		KOI_LOG_DEBUG("LightingPass: (re)building light gather system for new world");

		m_gatherSystem = m_world->GetECS()
			.system<const Transform, const Light>("LightGatherSystem")
			.kind(0)  // never auto-scheduled by world.progress() | only invoked via run() below
			.each([this](flecs::iter& it, size_t, const Transform& xform, const Light& light)
			{
				std::vector<GPULight>* lights = it.param<std::vector<GPULight>>();

				// Directional lights use the entity's forward; point lights use its position.
				Vector3 forward = Matrix::CreateFromQuaternion(xform.Rotation).Forward();
				forward.Normalize();

				// ShadowPass picks its caster with the same rule, so the first match here is
				// the light its map was rendered from
				const bool shadowed = m_shadowMapIndex != INVALID_DESCRIPTOR
					&& light.CastsShadows
					&& light.Type == LightType::Directional;

				lights->push_back(GPULight
				{
					.Position       = xform.Position,
					.Type           = static_cast<u32>(light.Type),
					.Direction      = forward,
					.Range          = light.Range,
					.Color          = light.Color,
					.Intensity      = light.Intensity,
					.ShadowMatrix   = shadowed ? m_lightViewProjection : Matrix::Identity,
					.ShadowMapIndex = shadowed ? m_shadowMapIndex : INVALID_DESCRIPTOR,
					.ShadowBias     = light.ShadowBias,
					._Pad           = {},
				});

				if (shadowed) m_shadowMapIndex = INVALID_DESCRIPTOR;  // one shadowed light per frame
			});
	}

	void LightingPass::Execute(CommandList& cmd)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		if (!m_world) return;

		GfxDevice& device = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		// Rebuild the gather system if the world was first encountered or changed.
		if (flecs::world_t* ecsWorld = m_world->GetECS().c_ptr(); m_queryWorld != ecsWorld)
		{
			m_queryWorld = ecsWorld;
			ConfigureECS();
		}

		// Resolved before the gather so the lambda can stamp it onto the casting light
		m_shadowMapIndex = INVALID_DESCRIPTOR;
		if (const Texture* shadowTex = device.Get(m_inputs[size_t(Input::Shadow)].Texture))
			m_shadowMapIndex = shadowTex->SRVIndex;

		m_lights.clear();
		m_gatherSystem.run(0.0f, &m_lights);

		m_lightCount = static_cast<u32>(m_lights.size());
		if (m_lights.empty()) return;

		const DescriptorIndex lightBufferIndex = m_lightBuffer.Upload(frameSlot, std::span<const GPULight>(m_lights));

		// --- Full-screen lighting draw ---
		if (!m_colorTarget || !m_camera || !m_config.Resources || !m_config.Resources->LightingPipeline)
			return;

		const Texture* albedoTex   = device.Get(m_inputs[size_t(Input::Albedo)].Texture);
		const Texture* normalTex   = device.Get(m_inputs[size_t(Input::Normals)].Texture);
		const Texture* materialTex = device.Get(m_inputs[size_t(Input::Material)].Texture);
		const Texture* depthTex    = device.Get(m_inputs[size_t(Input::Depth)].Texture);
		if (!albedoTex || !normalTex || !materialTex || !depthTex) return;

		// G-buffer outputs and the shadow map were left shader-readable by the passes that wrote them
		// only the HDR target needs transitioning
		cmd.TransitionBarrier(m_colorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd.FlushBarriers();

		cmd.SetRenderTarget(m_colorTarget);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(m_config.Resources->LightingPipeline);

		LightingRootConstants pc
		{
			.InvViewProjection  = m_camera->ViewProjection.Invert(),
			.CameraPosition     = Vector4(m_camera->Position.x, m_camera->Position.y, m_camera->Position.z, 0.0f),
			.AlbedoIndex        = albedoTex->SRVIndex,
			.NormalIndex        = normalTex->SRVIndex,
			.MaterialIndex      = materialTex->SRVIndex,
			.DepthIndex         = depthTex->SRVIndex,
			.LightBufferIndex   = lightBufferIndex,
			.LightCount         = m_lightCount,
			.ShadowSamplerIndex = m_config.Resources->ShadowSampler,
			._Pad               = 0,
		};
		cmd.SetRootConstants(&pc, sizeof(pc) / sizeof(u32));
		cmd.Draw(3);
	}
}
