#include "Koi/Engine/Graphics/TargetCapture.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/RHI/Device.h"

namespace koi
{
	namespace
	{
		constexpr RasterPipeline kCapture
		{
			.VertexShader = "engine://Content/Shaders/TonemapVS",
			.PixelShader  = "engine://Content/Shaders/DebugVisualizePS",
			.Rasterizer   = RasterizerDesc::CullNone(),
			.DepthStencil = DepthStencilDesc::None(),
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
			.Usage     = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
			.DebugName = "Capture Preview",
		});
	}

	void TargetCapture::OnWrite(RenderGraph& graph, std::string_view resource, RGTexture written)
	{
		if (!m_target || resource != m_target->Name || written.Version != m_target->Version) return;

		const TextureDesc& desc = graph.GetDesc(written);
		if (!desc.Usage.Has(TextureUsage::ShaderRead)) return;

		ZoneScoped;
		EnsurePreview(desc.Width, desc.Height);
		RGTexture preview = graph.ImportTexture(m_preview, "Capture");

		// Full-screen quad pass
		graph.AddPass<RasterPass>("Capture",
			[&](RGBuilder& b)
			{
				b.Read(written);
				preview = b.ColorTarget(preview);
			},
			[written, settings = m_settings, nearZ = m_near, farZ = m_far](RasterContext& ctx)
			{
				if (!ctx.SetPipeline(kCapture)) return;

				ctx.SetRootConstants(CaptureRootConstants
				{
					.SourceIndex = ctx.SRV(written),
					.Mode        = static_cast<u32>(settings.Mode),
					.ChannelMask = settings.Channels,
					.RangeMin    = settings.RangeMin,
					.RangeMax    = settings.RangeMax,
					.Near        = nearZ,
					.Far         = farZ,
					._Pad        = 0,
				});
				ctx.Draw(3);
			});

		m_capturedFrame = m_device->GetCurrentFrameIndex();
	}
}
