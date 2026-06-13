#include "Graphics/RenderPasses/TonemapPass.h"
#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/CommandList.h"

namespace koi
{
	TonemapPass::TonemapPass(const Desc& desc)
		: m_desc(desc)
		, m_input{ desc.HdrInput, "HDRColor" }
	{
	}

	std::span<const RenderPassAttachment> TonemapPass::GetInputs() const
	{
		return { &m_input, 1 };
	}

	void TonemapPass::Execute(CommandList& cmd)
	{
		RenderWindow* rw = m_desc.TargetWindow;
		if (!rw || !m_desc.Pipeline || !m_desc.Device) return;

		cmd.TransitionBarrier(m_desc.HdrInput, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.TransitionBarrier(rw->CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);

		cmd.SetRenderTarget(rw->CurrentBackBuffer());
		cmd.SetViewportAndScissor(rw->GetWidth(), rw->GetHeight());
		cmd.SetPipeline(m_desc.Pipeline);

		const Texture* hdrTex = m_desc.Device->Get(m_desc.HdrInput);
		u32 hdrSrvIndex = hdrTex ? hdrTex->SRVIndex : INVALID_DESCRIPTOR;
		cmd.SetPushConstants(&hdrSrvIndex, 1);
		cmd.Draw(3);
	}
}
