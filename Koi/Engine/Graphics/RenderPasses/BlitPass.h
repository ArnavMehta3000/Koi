#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"

namespace koi
{
	class BlitPass
	{
	public:
		static constexpr std::string_view kName = "Blit";

		// Copies DisplayColor into the view's target
		void Record(RenderGraph& graph, const RenderView& view, World& world);
	};
}
