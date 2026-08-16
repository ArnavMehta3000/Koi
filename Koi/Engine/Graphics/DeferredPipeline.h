#pragma once
#include "Koi/Engine/Graphics/IRenderPipeline.h"

namespace koi
{
	class GBufferPass;
	class LightingPass;
	class TonemapPass;

	class DeferredPipeline : public IRenderPipeline
	{
	public:
		void Build(Renderer& renderer, RenderGraph& graph) override;
		void Setup(const RenderView& view, World& world) override;

	private:
		GBufferPass*  m_gbufferPass  = nullptr;
		LightingPass* m_lightingPass = nullptr;
		TonemapPass*  m_tonemapPass  = nullptr;
	};
}
