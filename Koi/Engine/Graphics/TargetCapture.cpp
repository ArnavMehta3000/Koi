#include "Koi/Engine/Graphics/TargetCapture.h"
#include "Koi/Engine/Content/Shaders/Interop/PostProcess.hlsli"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/Device.h"

namespace koi
{
	namespace
	{
		constexpr ComputePipeline kCapture
		{
			.ComputeShader = "engine://Content/Shaders/DebugVisualizeCS",
		};
	}

	void TargetCapture::Init(GfxDevice& device)
	{
		m_device = &device;
	}

	void TargetCapture::Shutdown()
	{
		if (m_device && m_preview)
		{
			m_device->Destroy(m_preview);
		}
		m_preview = {};
		m_target.reset();
	}

	TextureHandle TargetCapture::GetPreview() const
	{
		return m_device && m_capturedFrame == m_device->GetCurrentFrameIndex() ? m_preview : TextureHandle();
	}

	void TargetCapture::EnsurePreview(u32 width, u32 height)
	{
		if (const Texture* tex = m_device->Get(m_preview); tex && tex->Desc.Width == width && tex->Desc.Height == height)
			return;  // Valid texture already loaded/created

		if (m_preview)
		{
			// Destroy existing preview texture
			m_device->Destroy(m_preview);
		}

		// Create new copy resource texture
		m_preview = m_device->CreateTexture(TextureDesc
		{
			.Width     = width,
			.Height    = height,
			.Format    = Format::R16G16B16A16_FLOAT,
			.Usage     = TextureUsage::ShaderWrite | TextureUsage::ShaderRead,
			.DebugName = "Capture Preview",
		});
	}

	void TargetCapture::OnWrite(RenderGraph& graph, std::string_view resource, RGTexture written)
	{
		if (!m_target || resource != m_target->Name || written.Version != m_target->Version) return;

		const TextureDesc& desc = graph.GetDesc(written);
		if (!desc.Usage.Has(TextureUsage::ShaderRead)) return;

		ZoneScoped;
		// Copied to prevent desc dangling post import
		const u32 width  = desc.Width;
		const u32 height = desc.Height;
		EnsurePreview(width, height);
		RGTexture preview = graph.ImportTexture(m_preview, "Capture");

		graph.AddPass<ComputePass>("Capture",
			[&](RGBuilder& b)
			{
				b.Read(written);
				preview = b.Write(preview);
			},
			[written, preview, width, height, settings = m_settings, nearZ = m_near, farZ = m_far](ComputeContext& ctx)
			{
				if (!ctx.SetPipeline(kCapture)) return;

				ctx.SetRootConstants(CaptureRootConstants
				{
					.SourceIndex  = ctx.SRV(written),
					.PreviewIndex = ctx.UAV(preview),
					.Mode         = static_cast<u32>(settings.Mode),
					.ChannelMask  = settings.Channels,
					.RangeMin     = settings.RangeMin,
					.RangeMax     = settings.RangeMax,
					.Near         = nearZ,
					.Far          = farZ,
				});

				const auto& group = ctx.GetThreadGroup();
				ctx.Dispatch((width + group[0] - 1) / group[0], (height + group[1] - 1) / group[1]);
			});

		m_capturedFrame = m_device->GetCurrentFrameIndex();
	}
}
