#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/RHI/RenderGraph/RenderGraph.h"
#include <string>
#include <string_view>

namespace koi
{
	class CommandList;
	class Renderer;
	class RenderScene;
	class TargetCapture;
	struct RenderView;

	class SceneRenderer : NonCopyable, NonMovable
	{
	public:
		SceneRenderer(Renderer& renderer, std::string_view pipeline);
		~SceneRenderer();

		void SetPipeline(std::string_view pipeline);
		[[nodiscard]] const std::string& GetPipeline() const noexcept { return m_pipeline; }

		void Submit(const RenderView& view, const RenderScene& scene);

		// What the last submitted view rendered into
		[[nodiscard]] TextureHandle GetOutput() const noexcept { return m_output; }

		// Inspection and capture follow the graph this is set on
		void SetCapture(TargetCapture* capture);

		[[nodiscard]] FeatureList&       GetFeatures() noexcept          { return m_features; }
		[[nodiscard]] const RenderGraph& GetRenderGraph() const noexcept { return m_graph; }

	private:
		friend class Renderer;
		void Execute(CommandList& cmd);

	private:
		Renderer*      m_renderer = nullptr;
		std::string    m_pipeline;
		FeatureList    m_features;
		RenderGraph    m_graph;
		TargetCapture* m_capture = nullptr;
		TextureHandle  m_output;
	};
}
