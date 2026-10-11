#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class TonemapPass : public RenderPass
	{
	public:
		inline std::string_view GetName() const override { return "Tonemap"; }

		// Returns the version of output it wrote
		RGTexture Record(RenderGraph& graph, RGTexture hdr, RGTexture output);
	};
}
