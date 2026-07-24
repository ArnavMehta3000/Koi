#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	GBufferPass::GBufferPass(const Config& config)
		: m_config(config)
	{
		assert(config.Device && "GBufferPass requires a valid GfxDevice");
	}

	GBufferPass::~GBufferPass()
	{
		if (m_config.Device) DestroyRenderTargets();
	}

	void GBufferPass::Execute(CommandList& cmd)
	{
		// TODO
	}

	void GBufferPass::Reset()
	{
		// Clear only transient per-frame inputs
		m_world  = nullptr;
		m_camera = nullptr;
	}

	GBufferPass& GBufferPass::SetTarget(u32 width, u32 height)
	{
		if (width == m_width && height == m_height) return *this;

		m_width  = width;
		m_height = height;

		// Release the old targets and recreate at the new size
		DestroyRenderTargets();
		CreateRenderTargets();
		return *this;
	}

	void GBufferPass::DestroyRenderTargets()
	{
		for (RenderPassAttachment& o : m_outputs)
		{
			if (o.Texture) m_config.Device->Destroy(o.Texture);
			o.Texture = TextureHandle();
		}
	}

	void GBufferPass::CreateRenderTargets()
	{
		TextureDesc baseTexDesc
		{
			.Width  = m_width,
			.Height = m_height,
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
