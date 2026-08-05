#include "Koi/Engine/Graphics/RenderPasses/TonemapPass.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"

namespace koi
{
	TonemapPass::TonemapPass(const Config& config)
		: m_config(config)
		, m_input{ {}, "HDRColor" }
	{
	}

	void TonemapPass::Reset()
	{
		m_input.Texture = {};
		m_colorTarget   = {};
		m_width         = 0;
		m_height        = 0;
	}

	TonemapPass& TonemapPass::SetTarget(TextureHandle colorTarget, u32 width, u32 height)
	{
		m_colorTarget = colorTarget;
		m_width       = width;
		m_height      = height;
		return *this;
	}

	void TonemapPass::Execute(CommandList& cmd)
	{
		if (!m_colorTarget || !m_config.Pipeline || !m_config.Device) return;

		cmd.TransitionBarrier(m_input.Texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.TransitionBarrier(m_colorTarget, D3D12_RESOURCE_STATE_RENDER_TARGET);

		cmd.SetRenderTarget(m_colorTarget);
		cmd.SetViewportAndScissor(m_width, m_height);
		cmd.SetPipeline(m_config.Pipeline);

		const Texture* hdrTex = m_config.Device->Get(m_input.Texture);
		u32 hdrSrvIndex       = hdrTex ? hdrTex->SRVIndex : INVALID_DESCRIPTOR;

		cmd.SetRootConstants(&hdrSrvIndex, 1);
		cmd.Draw(3);
	}
}
