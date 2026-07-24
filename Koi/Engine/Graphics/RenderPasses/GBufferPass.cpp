#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	GBufferPass::GBufferPass(const Config& config)
		: m_config(config)
	{
		assert(config.Device && "GBufferPass requires a valid GfxDevice");
		assert(config.Width > 0 && config.Height > 0 && "Invalid GBuffer RT dimensions");

		CreateRenderTargets();
	}

	void GBufferPass::Execute(CommandList& cmd)
	{
		// TODO
	}

	void GBufferPass::Reset()
	{
		for (RenderPassAttachment& o : m_outputs)
		{
			m_config.Device->Destroy(o.Texture);
			o.Texture = TextureHandle();
		}
	}

	void GBufferPass::SetRTSize(u32 width, u32 height)
	{
		m_config.Width  = width;
		m_config.Height = height;

		// Recreate/resize render targets
		Reset();
		CreateRenderTargets();
	}

	void GBufferPass::CreateRenderTargets()
	{
		TextureDesc baseTexDesc
		{
			.Width  = m_config.Width,
			.Height = m_config.Height,
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

		m_outputs[size_t(Outputs::Albedo)]   = { albedoSRV  , "Albedo" };
		m_outputs[size_t(Outputs::Normals)]  = { normalSRV  , "Normals" };
		m_outputs[size_t(Outputs::Material)] = { materialSRV, "Material" };
	}
}
