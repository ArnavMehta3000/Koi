#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class TonemapPass : public RenderPass
	{
	public:
		// Stable for the pass's lifetime - set once at construction.
		struct Config
		{
			PipelineHandle Pipeline;
		};

		explicit TonemapPass(const Config& config) : m_config(config) {}

		inline std::string_view GetName() const override { return "Tonemap"; }

		// Returns the version of output it wrote
		RGTexture Record(RenderGraph& graph, RGTexture hdr, RGTexture output);

	private:
		Config m_config;
	};
}
