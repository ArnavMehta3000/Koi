#pragma once
#include "Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class GfxDevice;
	class RenderWindow;

	class TonemapPass : public RenderPass
	{
	public:
		struct Desc
		{
			GfxDevice*     Device       = nullptr;
			PipelineHandle Pipeline;
			TextureHandle  HdrInput;
			RenderWindow*  TargetWindow = nullptr;
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
