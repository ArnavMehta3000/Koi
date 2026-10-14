#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class BlitPass : public RenderPass
	{
	public:
		inline std::string_view GetName() const override { return "Blit"; }

		RGTexture Record(RenderGraph& graph, RGTexture source, RGTexture output);
	};
}
