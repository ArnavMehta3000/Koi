#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"

namespace koi
{
	class TonemapPass
	{
	public:
		static constexpr std::string_view kName = "Tonemap";

		// Reads SceneColor, publishes DisplayColor
		void Record(RenderGraph& graph, const RenderView& view, World& world);
	};
}
