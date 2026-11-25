#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"

namespace koi
{
	class  RenderScene;
	struct RendererResources;

	class SkyPass
	{
	public:
		static constexpr std::string_view kName = "Sky";

		struct Config
		{
			const RendererResources* Resources = nullptr;
		};

		explicit SkyPass(const Config& config) : m_config(config) {}
		void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene);

	private:
		Config m_config;
	};
}
