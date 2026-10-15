#include "Koi/Engine/Graphics/RenderPasses/LightingPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <cassert>

namespace koi
{
	namespace
	{
		constexpr RasterPipeline kLighting
		{
			.VertexShader = "engine://Content/Shaders/LightingVS",
			.PixelShader  = "engine://Content/Shaders/LightingPS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.DepthStencil = DepthStencilDesc::None(),
		};
	}

	LightingPass::LightingPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		assert(m_config.Device && "LightingPass requires a valid GfxDevice");

		m_lightBuffer.Init(*m_config.Device, BufferUsage::Storage, 16, "GPULight");
	}

	void LightingPass::Record(RenderGraph& graph, World& world, const Camera& camera)
	{
		ZoneScoped;
		RGBlackboard& bb = graph.GetBlackboard();

		const GBufferData* gbufferData = bb.TryGet<GBufferData>();
		const SceneDepth*  sceneDepth  = bb.TryGet<SceneDepth>();
		if (!gbufferData || !sceneDepth) return;

		const GBufferData gbuffer = *gbufferData;
		const RGTexture   depth   = sceneDepth->Depth;
		const ShadowData  shadow  = bb.Has<ShadowData>() ? bb.Get<ShadowData>() : ShadowData{};

		const TextureDesc size = graph.GetDesc(gbuffer.Albedo);
		RGTexture hdr = graph.CreateTexture(TextureDesc{ .Width = size.Width, .Height = size.Height, .Format = Format::R16G16B16A16_FLOAT }, "HDRColor");

		graph.AddPass<RasterPass>("Lighting",
			[&](RGBuilder& b)
			{
				b.Read(gbuffer.Albedo);
				b.Read(gbuffer.Normals);
				b.Read(gbuffer.Material);
				b.Read(depth);
				b.Read(shadow.ShadowMap);
				hdr = b.ColorTarget(hdr, LoadOp::Clear);
			},
			[this, &world, &camera, gbuffer, depth, shadow](RasterContext& ctx)
			{
				GfxDevice& device   = *m_config.Device;
				const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

				GatherLights(world, shadow.LightViewProjection, ctx.SRV(shadow.ShadowMap));
				if (m_lights.empty() || !m_config.Resources || !ctx.SetPipeline(kLighting)) return;

				const DescriptorIndex lightBufferIndex = m_lightBuffer.Upload(frameSlot, std::span<const GPULight>(m_lights));

				ctx.SetRootConstants(LightingRootConstants
				{
					.InvViewProjection  = camera.ViewProjection.Invert(),
					.CameraPosition     = Vector4(camera.Position.x, camera.Position.y, camera.Position.z, 0.0f),
					.AlbedoIndex        = ctx.SRV(gbuffer.Albedo),
					.NormalIndex        = ctx.SRV(gbuffer.Normals),
					.MaterialIndex      = ctx.SRV(gbuffer.Material),
					.DepthIndex         = ctx.SRV(depth),
					.LightBufferIndex   = lightBufferIndex,
					.LightCount         = static_cast<u32>(m_lights.size()),
					.ShadowSamplerIndex = m_config.Resources->ShadowSampler,
					._Pad               = 0,
				});
				ctx.Draw(3);
			});

		bb.Add(SceneColor{ .Color = hdr });
	}

	void LightingPass::GatherLights(World& world, const Matrix& lightViewProjection, DescriptorIndex shadowMapIndex)
	{
		ZoneScoped;
		m_lights.clear();

		world.GetECS().each([&](const Transform& xform, const Light& light)
		{
			// Directional lights use the entity's forward; point lights use its position
			Vector3 forward = Matrix::CreateFromQuaternion(xform.Rotation).Forward();
			forward.Normalize();

			// ShadowPass picks its caster with the same rule, so the first match here is
			// the light its map was rendered from
			const bool shadowed = shadowMapIndex != INVALID_DESCRIPTOR
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
				.ShadowMatrix   = shadowed ? lightViewProjection : Matrix::Identity,
				.ShadowMapIndex = shadowed ? shadowMapIndex : INVALID_DESCRIPTOR,
				.ShadowBias     = light.ShadowBias,
				._Pad           = {},
			});

			if (shadowed) shadowMapIndex = INVALID_DESCRIPTOR;  // one shadowed light per frame
		});
	}
}
