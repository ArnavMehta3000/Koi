#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include <array>

namespace koi
{
	class  Scene;
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

		inline ScenePass& SetScene(Scene* scene) { m_scene = scene; return *this; }
		inline ScenePass& SetCamera(const Camera* camera) { m_camera = camera; return *this; }
		ScenePass& SetTarget(TextureHandle colorTarget, TextureHandle depthTarget, u32 width, u32 height);

		[[nodiscard]] inline TextureHandle GetColorOutput() const { return m_outputs[0].Texture; }
		[[nodiscard]] inline TextureHandle GetDepthOutput() const { return m_outputs[1].Texture; }

	private:
		Config        m_config;
		Scene*        m_scene  = nullptr;
		const Camera* m_camera = nullptr;
		u32           m_width  = 0;
		u32           m_height = 0;
		std::array<RenderPassAttachment, 2> m_outputs;
	};
}
