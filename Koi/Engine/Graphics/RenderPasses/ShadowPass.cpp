#include "Koi/Engine/Graphics/RenderPasses/ShadowPass.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/RenderScene.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/RHI/Device.h"

namespace koi
{
	namespace
	{
		// Raster-stage depth bias pushes fragments away from the light, which removes most acne for free
		constexpr RasterPipeline MakeShadow(bool masked, bool doubleSided)
		{
			return
			{
				.VertexShader = masked ? ShaderRef("engine://Content/Shaders/ShadowMaskedVS") : ShaderRef("engine://Content/Shaders/ShadowVS"),
				.PixelShader  = masked ? ShaderRef("engine://Content/Shaders/ShadowMaskedPS") : ShaderRef(),
				.Rasterizer   =
				{
					.CullMode             = doubleSided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK,
					.DepthBias            = 100,
					.SlopeScaledDepthBias = 2.0f,
				},
			};
		}

		// Indexed [masked][double sided]
		constexpr RasterPipeline kShadow[2][2] =
		{
			{ MakeShadow(false, false), MakeShadow(false, true) },
			{ MakeShadow(true,  false), MakeShadow(true,  true) },
		};
	}

	ShadowPass::ShadowPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		KOI_ASSERT(config.Device, "ShadowPass requires a valid GfxDevice");

		m_frameConstants.Init(*m_config.Device, BufferUsage::Constant, 1,  "Shadow FrameConstants");
		m_objects.Init      (*m_config.Device, BufferUsage::Storage,  64, "Shadow PerObjectData");
	}

	void ShadowPass::Record(RenderGraph& graph, const RenderView&, const RenderScene& scene)
	{
		ZoneScoped;
		m_hasShadow = ComputeLightMatrix(scene);
		if (!m_hasShadow || !m_config.Resources) return;

		RGTexture shadowMap = graph.CreateTexture(TextureDesc
		{
			.Width  = m_config.Resolution,
			.Height = m_config.Resolution,
			.Format = Format::D32_FLOAT,
		}, "ShadowMap");

		// Cleared even with nothing to draw: Lighting samples the map whenever it exists
		graph.AddPass<RasterPass>("Shadow",
			[&](RGBuilder& b) { shadowMap = b.DepthTarget(shadowMap, LoadOp::Clear); },
			[this, &scene](RasterContext& ctx) { Draw(ctx, scene); });

		graph.GetBlackboard().Add(ShadowData{ .ShadowMap = shadowMap, .LightViewProjection = m_lightViewProjection });
	}

	bool ShadowPass::ComputeLightMatrix(const RenderScene& scene)
	{
		// Returns false when there is no caster

		ZoneScoped;

		Vector3 lightDir;
		Vector3 center;
		f32     radius = 0.0f;
		bool    found  = false;

		for (const RenderLight& light : scene.GetLights())
		{
			if (!light.Data.CastsShadows || light.Data.Type != LightType::Directional) continue;

			lightDir = light.Direction;
			center   = light.Position;
			radius   = light.Data.ShadowExtent;
			found    = true;
			break;
		}

		if (!found) return false;

		// No extent on the light: fit the box to the scene's bounding sphere
		// Texel density falls off as the scene grows
		if (radius <= 0.0f)
		{
			const BoundingBox& bounds = scene.GetBounds();
			if (math::IsEmpty(bounds)) return false;  // nothing loaded yet

			center = Vector3(bounds.Center);
			radius = math::BoundsRadius(bounds);
		}

		const Vector3 eye = center - lightDir * (radius * 2.0f);

		// Any up vector works except one parallel to the light
		const Vector3 up = (lightDir.y > 0.99f || lightDir.y < -0.99f) ? Vector3::Right : Vector3::Up;

		const Matrix view = Matrix::CreateLookAt(eye, center, up);
		const Matrix proj = Matrix::CreateOrthographic(radius * 2.0f, radius * 2.0f, 0.0f, radius * 4.0f);

		m_lightViewProjection = view * proj;

		// The ortho volume as a world-space box. BoundingFrustum only understands perspective
		// projections, so a directional light has to cull against a box instead
		const Matrix invView = view.Invert();

		BoundingOrientedBox volume;
		BoundingOrientedBox::CreateFromBoundingBox(volume,
			BoundingBox(
				Vector3(0.0f, 0.0f, -radius * 2.0f),
				Vector3(radius, radius, radius * 2.0f)));

		volume.Transform(volume, invView);

		m_cullVolume = CullVolume(volume);
		return true;
	}

	void ShadowPass::Draw(RasterContext& ctx, const RenderScene& scene)
	{
		ZoneScoped;
		GfxDevice& device    = *m_config.Device;
		const u32  frameSlot = device.GetFrameSlot();

		// Depth-only, so the normal matrix is never needed
		constexpr GatherDesc kGather{ .Require = RenderFlag::CastsShadow, .Normals = false };
		scene.Gather(kGather, m_cullVolume, m_gather);
		FrameStats::Count("Shadow Draws",  i64(m_gather.Draws.size()));
		FrameStats::Count("Shadow Culled", i64(m_gather.Culled));
		if (m_gather.Draws.empty()) return;

		const DescriptorIndex objectBufferIndex = m_objects.Upload(frameSlot, std::span<const PerObjectData>(m_gather.Objects));

		const DescriptorIndex frameConstantsIndex = m_frameConstants.Upload(frameSlot, FrameConstants
		{
			.ViewProjection      = m_lightViewProjection,
			.CameraPosition      = Vector4::Zero,
			.MaterialBufferIndex = m_config.Materials ? m_config.Materials->Upload(frameSlot) : INVALID_DESCRIPTOR,
		});

		const GPUMesh*        boundMesh     = nullptr;
		const RasterPipeline* boundPipeline = nullptr;
		bool                  pipelineOk    = false;

		for (const MeshDrawItem& item : m_gather.Draws)
		{
			KOI_ASSERT(item.Mesh, "Invalid mesh pointer");

			const RasterPipeline* pipeline = &kShadow[item.Masked && m_config.Materials][item.DoubleSided];
			if (pipeline != boundPipeline)
			{
				pipelineOk    = ctx.SetPipeline(*pipeline);
				boundPipeline = pipeline;
			}
			if (!pipelineOk) continue;

			if (item.Mesh && item.Mesh != boundMesh)
			{
				ctx.SetIndexBuffer(item.Mesh->IndexBuffer);
				boundMesh = item.Mesh;
			}

			const GPUMeshDraw& d = *item.Draw;
			ctx.SetRootConstants(DrawRootConstants
			{
				.FrameConstantsIndex = frameConstantsIndex,
				.ObjectBufferIndex   = objectBufferIndex,
				.ObjectIndex         = item.ObjectIndex,
				.MaterialIndex       = item.MaterialSlot,
				.VertexBufferIndex   = item.Mesh ? item.Mesh->VertexBufferSRV : INVALID_DESCRIPTOR,
				.SamplerIndex        = m_config.Resources->LinearWrapSampler,
				.VertexOffset        = static_cast<u32>(d.VertexOffset),
			});
			ctx.DrawIndexed(d.IndexCount, 1, d.IndexOffset, 0);
		}
	}
}
