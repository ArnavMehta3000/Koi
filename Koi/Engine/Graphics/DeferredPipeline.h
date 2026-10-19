#pragma once

namespace koi
{
	class FeatureList;
	class Renderer;

	struct DeferredPipeline
	{
		static void Build(Renderer& renderer, FeatureList& features);
	};
}
