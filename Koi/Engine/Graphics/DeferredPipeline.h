#pragma once
#include "Koi/Engine/Graphics/IRenderPipeline.h"

namespace koi
{
	class GBufferPass;
	class LightingPass;
	class ShadowPass;
	class TonemapPass;

	class DeferredPipeline : public IRenderPipeline
	{
	public:
		void Build(Renderer& renderer, RenderGraph& graph) override;
		void Setup(const RenderView& view, World& world) override;

	private:
		ShadowPass*   m_shadowPass   = nullptr;
		GBufferPass*  m_gbufferPass  = nullptr;
		LightingPass* m_lightingPass = nullptr;
		TonemapPass*  m_tonemapPass  = nullptr;
	};
}
