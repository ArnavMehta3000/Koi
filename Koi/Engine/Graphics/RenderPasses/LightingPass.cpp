#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include <array>
#include <cassert>

namespace koi
{
	LightingPass::LightingPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		assert(m_config.Device && "LightingPass requires a valid GfxDevice");

		m_lightBuffer.Init(*m_config.Device, BufferUsage::Storage, 16, "GPULight");
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
		m_inputs         = {};
	}

	LightingPass& LightingPass::SetGBuffer(TextureHandle albedo, TextureHandle normals, TextureHandle material, TextureHandle depth)
	{
		m_inputs.Albedo   = albedo;
		m_inputs.Normals  = normals;
		m_inputs.Material = material;
		m_inputs.Depth    = depth;
		return *this;
	}

	LightingPass& LightingPass::SetShadow(TextureHandle shadowMap, const Matrix& lightViewProjection)
	{
		m_inputs.Shadow       = shadowMap;
		m_lightViewProjection = lightViewProjection;
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

	void LightingPass::GatherLights()
	{
		ZoneScoped;
		m_lights.clear();

		m_world->GetECS().each([this](const Transform& xform, const Light& light)
		{
			// Directional lights use the entity's forward; point lights use its position
			Vector3 forward = Matrix::CreateFromQuaternion(xform.Rotation).Forward();
			forward.Normalize();

			// ShadowPass picks its caster with the same rule, so the first match here is
			// the light its map was rendered from
			const bool shadowed = m_shadowMapIndex != INVALID_DESCRIPTOR
				&& light.CastsShadows
				&& light.Type == LightType::Directional;

			m_lights.push_back(GPULight
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
		if (!m_world) return;

		GfxDevice& device = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		// Resolved before the gather so it can be stamped onto the casting light
		m_shadowMapIndex = INVALID_DESCRIPTOR;
		if (const Texture* shadowTex = device.Get(m_inputs.Shadow))
		{
			m_shadowMapIndex = shadowTex->SRVIndex;
		}

		GatherLights();

		m_lightCount = static_cast<u32>(m_lights.size());
		if (m_lights.empty()) return;

		const DescriptorIndex lightBufferIndex = m_lightBuffer.Upload(frameSlot, std::span<const GPULight>(m_lights));

		// --- Full-screen lighting draw ---
		if (!m_colorTarget || !m_camera || !m_config.Resources || !m_config.Resources->LightingPipeline)
			return;

		const Texture* albedoTex   = device.Get(m_inputs.Albedo);
		const Texture* normalTex   = device.Get(m_inputs.Normals);
		const Texture* materialTex = device.Get(m_inputs.Material);
		const Texture* depthTex    = device.Get(m_inputs.Depth);
		if (!albedoTex || !normalTex || !materialTex || !depthTex) return;

		const std::array inputs = { m_inputs.Albedo, m_inputs.Normals, m_inputs.Material, m_inputs.Depth, m_inputs.Shadow };
		cmd.TransitionBarriers(inputs, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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
