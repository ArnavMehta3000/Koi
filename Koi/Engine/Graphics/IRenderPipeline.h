#pragma once
#include <memory>
#include <string_view>

namespace koi
{
	class RenderGraph;
	class Renderer;
	class World;
	struct RenderView;

	// Owns a pass set and the per-frame wiring between those passes
	class IRenderPipeline
	{
	public:
		virtual ~IRenderPipeline() = default;

		virtual void Build(Renderer& renderer, RenderGraph& graph) = 0;  // Pipeline init setup
		virtual void Setup(const RenderView& view, World& world)   = 0;  // Per-frame setup
	};

	// Named by the project TOML ([rendering] pipeline) | fallback: "deferred"
	[[nodiscard]] std::unique_ptr<IRenderPipeline> CreateRenderPipeline(std::string_view name);
}
