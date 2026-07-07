#include "Koi/Engine/Graphics/RenderPasses/GBufferPass.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	GBufferPass::GBufferPass(const Desc& desc)
		: m_desc(desc)
	{
		auto& engine = Engine::Get();
		auto& window = engine.GetWindow();

		const auto& [w, h] = std::make_pair(window.GetWidth(), window.GetHeight());
		auto& device = engine.GetRenderer().GetDevice();

		TextureDesc baseTexDesc
		{
			.Width = w,
			.Height = h,
			.Format = Format::R8G8B8A8_UNORM,
			.Usage = TextureUsage::RenderTarget
		};

		baseTexDesc.DebugName   = "GBuffer Albedo";
		TextureHandle albedoSRV = device.CreateTexture(baseTexDesc);

		baseTexDesc.Format      = Format::R16G16B16A16_UNORM;
		baseTexDesc.DebugName   = "GBuffer Normal";
		TextureHandle normalSRV = device.CreateTexture(baseTexDesc);

		baseTexDesc.Format        = Format::R8G8B8A8_UNORM;
		baseTexDesc.DebugName     = "GBuffer Material";
		TextureHandle materialSRV = device.CreateTexture(baseTexDesc);

		baseTexDesc.Format     = Format::R32_FLOAT;
		baseTexDesc.Usage      = TextureUsage::DepthStencil;
		baseTexDesc.DebugName  = "GBuffer Depth";
		TextureHandle depthSRV = device.CreateTexture(baseTexDesc);

		m_outputs[size_t(Outputs::Albedo)]   = { albedoSRV, "Albedo"     };
		m_outputs[size_t(Outputs::Normals)]  = { normalSRV, "Normals"    };
		m_outputs[size_t(Outputs::Material)] = { materialSRV, "Material" };
		m_outputs[size_t(Outputs::Depth)]    = { depthSRV, "Depth"       };
	}

	void GBufferPass::Execute(CommandList& cmd)
	{
		// TODO
	}
}
