#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/ECS/Components.h"
#include <flecs.h>
#include <array>

namespace koi
{
	class  World;
	struct Camera;
	struct RendererResources;

	class ScenePass : public RenderPass
	{
	public:
		struct Config
		{
			const RendererResources* Resources = nullptr;
			PipelineHandle           Pipeline;  // Optional override (falls back to Resources->MeshOpaquePipeline)
		};

		explicit ScenePass(const Config& config);

		inline std::string_view GetName() const override { return "Scene"; }
		inline std::span<const RenderPassAttachment> GetOutputs() const override { return m_outputs; }
		void Reset() override;
		void Execute(CommandList& cmd) override;

		inline ScenePass& SetWorld(World* world) { m_world = world; return *this; }
		inline ScenePass& SetCamera(const Camera* camera) { m_camera = camera; return *this; }
		ScenePass& SetTarget(TextureHandle colorTarget, TextureHandle depthTarget, u32 width, u32 height);

		[[nodiscard]] inline TextureHandle GetColorOutput() const { return m_outputs[0].Texture; }
		[[nodiscard]] inline TextureHandle GetDepthOutput() const { return m_outputs[1].Texture; }

	private:
		// Per-frame context passed to the mesh render system via system::run()'s param,
		// retrieved inside the system callback with it.param<RenderContext>().
		struct RenderContext
		{
			CommandList*             Cmd;
			const Camera*            Camera;
			const RendererResources* Resources;
			PipelineHandle           Pipeline;
		};

		Config        m_config;
		World*        m_world  = nullptr;
		const Camera* m_camera = nullptr;
		u32           m_width  = 0;
		u32           m_height = 0;
		std::array<RenderPassAttachment, 2> m_outputs;

		flecs::world_t* m_queryWorld = nullptr;  // raw ecs world the cached system below was built against
		flecs::system m_renderSystem;
	};
}
