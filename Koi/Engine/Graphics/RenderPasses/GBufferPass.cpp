#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include <cassert>

namespace koi
{
	GBufferPass::GBufferPass(const Config& config)
		: m_config(config)
	{
		ZoneScoped;
		assert(config.Device && "GBufferPass requires a valid GfxDevice");

		m_frameConstants.Init(*m_config.Device, BufferUsage::Constant, 1,  "GBuffer FrameConstants");
		m_objects.Init      (*m_config.Device, BufferUsage::Storage,  64, "GBuffer PerObjectData");
	}

	GBufferPass::Output GBufferPass::Record(RenderGraph& graph, World& world, const Camera& camera, u32 width, u32 height)
	{
		ZoneScoped;

		// Formats must match PipelineRecipe::GBuffer
		const auto target = [&](Format format, std::string_view name)
		{
			return graph.CreateTexture(TextureDesc{ .Width = width, .Height = height, .Format = format }, name);
		};

		Output out
		{
			.Albedo   = target(Format::R8G8B8A8_UNORM,     "Albedo"),
			.Normals  = target(Format::R16G16B16A16_UNORM, "Normals"),
			.Material = target(Format::R8G8B8A8_UNORM,     "Material"),
			.Depth    = target(Format::D32_FLOAT,          "Depth"),
		};

		graph.AddPass<RasterPass>("GBuffer",
			[&](RGBuilder& b)
			{
				out.Albedo   = b.ColorTarget(out.Albedo,   LoadOp::Clear);
				out.Normals  = b.ColorTarget(out.Normals,  LoadOp::Clear);
				out.Material = b.ColorTarget(out.Material, LoadOp::Clear);
				out.Depth    = b.DepthTarget(out.Depth,    LoadOp::Clear);
			},
			[this, &world, &camera](RasterContext& ctx) { Draw(ctx, world, camera); });

		return out;
	}

	void GBufferPass::Draw(RasterContext& ctx, World& world, const Camera& camera)
	{
		ZoneScoped;
		if (!m_config.Resources) return;

		GfxDevice& device   = *m_config.Device;
		const u32 frameSlot = static_cast<u32>(device.GetCurrentFrameIndex() % FRAMES_IN_FLIGHT);

		m_gatherer.Gather(world, CullVolume::FromCamera(camera.View, camera.Projection), true, m_gather);
		FrameStats::Count("GBuffer Draws",  i64(m_gather.Draws.size()));
		FrameStats::Count("GBuffer Culled", i64(m_gather.Culled));
		if (m_gather.Draws.empty()) return;

		const DescriptorIndex objectBufferIndex = m_objects.Upload(frameSlot, std::span<const PerObjectData>(m_gather.Objects));

		const DescriptorIndex frameConstantsIndex = m_frameConstants.Upload(frameSlot, FrameConstants
		{
			.ViewProjection      = camera.ViewProjection,
			.CameraPosition      = Vector4(camera.Position.x, camera.Position.y, camera.Position.z, 0.0f),
			.MaterialBufferIndex = m_config.Materials ? m_config.Materials->Upload(frameSlot) : INVALID_DESCRIPTOR,
		});

		// Draws arrive sorted by (pipeline, mesh), so both only rebind when they change.
		// A draw with no pipeline of its own gets the standard G-buffer one
		const GPUMesh* boundMesh = nullptr;
		PipelineHandle boundPipeline;

		for (const MeshDrawItem& item : m_gather.Draws)
		{
			const PipelineHandle pipeline = item.Pipeline.IsValid() ? item.Pipeline : m_config.Resources->GBufferPipeline;
			if (pipeline.GetId() != boundPipeline.GetId())
			{
				ctx.SetPipeline(pipeline);
				boundPipeline = pipeline;
			}

			if (item.Mesh != boundMesh)
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
				.VertexBufferIndex   = item.Mesh->VertexBufferSRV,
				.SamplerIndex        = m_config.Resources->LinearWrapSampler,
				.VertexOffset        = static_cast<u32>(d.VertexOffset),
			});
			ctx.DrawIndexed(d.IndexCount, 1, d.IndexOffset, 0);
		}
	}
}
