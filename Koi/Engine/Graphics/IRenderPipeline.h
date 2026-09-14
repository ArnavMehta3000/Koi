#pragma once
namespace koi
{
	class PassList;
	class Renderer;
	class World;
	struct RenderView;

	// Owns a pass set and the per-frame wiring between those passes
	class IRenderPipeline
	{
	public:
		virtual ~IRenderPipeline() = default;

		virtual void Build(Renderer& renderer, PassList& passes) = 0;  // Pipeline init setup
		virtual void Setup(const RenderView& view, World& world)  = 0;  // Per-frame setup
	};
}
