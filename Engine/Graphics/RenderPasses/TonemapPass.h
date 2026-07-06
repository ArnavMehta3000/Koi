#pragma once
#include "Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class GfxDevice;

	class TonemapPass : public RenderPass
	{
	public:
		struct Desc
		{
			GfxDevice*     Device      = nullptr;
			PipelineHandle Pipeline;
			TextureHandle  HdrInput;
			TextureHandle  ColorTarget;
			u32            Width       = 0;
			u32            Height      = 0;
		};

		explicit TonemapPass(const Desc& desc);

		std::string_view GetName() const override { return "Tonemap"; }
		std::span<const RenderPassAttachment> GetInputs() const override;
		void Execute(CommandList& cmd) override;

	private:
		Desc               m_desc;
		RenderPassAttachment m_input;
	};
}
