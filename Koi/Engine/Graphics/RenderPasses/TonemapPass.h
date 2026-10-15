#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"

namespace koi
{
	class TonemapPass : public RenderPass
	{
	public:
		inline std::string_view GetName() const override { return "Tonemap"; }

		// Reads SceneColor, publishes DisplayColor
		void Record(RenderGraph& graph);
	};
}
