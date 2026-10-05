#pragma once
#include "Koi/Engine/Graphics/IRenderPipeline.h"

namespace koi
{
	class DebugDrawPass;
	class GBufferPass;
	class GridPass;
	class LightingPass;
	class ShadowPass;
	class TonemapPass;

	class DeferredPipeline : public IRenderPipeline
	{
	public:
		void Build(Renderer& renderer, PassList& passes) override;
		void Setup(const RenderView& view, World& world) override;

	private:
		ShadowPass*    m_shadowPass    = nullptr;
		GBufferPass*   m_gbufferPass   = nullptr;
		LightingPass*  m_lightingPass  = nullptr;
		DebugDrawPass* m_debugDrawPass = nullptr;
		TonemapPass*   m_tonemapPass   = nullptr;
		GridPass*      m_gridPass      = nullptr;
	};
}
