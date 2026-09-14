#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class GfxDevice;

	class TonemapPass : public RenderPass
	{
	public:
		// Stable for the pass's lifetime - set once at construction.
		struct Config
		{
			GfxDevice*     Device = nullptr;
			PipelineHandle Pipeline;
		};

		explicit TonemapPass(const Config& config);

		inline std::string_view GetName() const override { return "Tonemap"; }
		void Reset() override;
		void Execute(CommandList& cmd) override;

		inline TonemapPass& SetInput(TextureHandle hdrInput) { m_input = hdrInput; return *this; }
		TonemapPass& SetTarget(TextureHandle colorTarget, u32 width, u32 height);

	private:
		Config               m_config;
		TextureHandle        m_input;
		TextureHandle        m_colorTarget;
		u32                  m_width  = 0;
		u32                  m_height = 0;
	};
}
