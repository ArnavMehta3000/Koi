#pragma once
#include "Graphics/RenderPasses/RenderPass.h"
#include <array>

namespace koi
{
	class  Scene;
	struct Camera;
	struct RendererResources;

	class ScenePass : public RenderPass
	{
	public:
		struct Desc
		{
			Scene*                   Scene       = nullptr;
			const Camera*            Camera      = nullptr;
			const RendererResources* Resources   = nullptr;
			TextureHandle            ColorTarget;
			TextureHandle            DepthBuffer;
			u32                      Width       = 0;
			u32                      Height      = 0;
			PipelineHandle           Pipeline;  // Optional override (falls back to Resources->MeshOpaquePipeline)
		};

		explicit ScenePass(const Desc& desc);

		std::string_view GetName() const override { return "Scene"; }
		std::span<const RenderPassAttachment> GetOutputs() const override { return m_outputs; }
		void Execute(CommandList& cmd) override;

	private:
		Desc m_desc;
		std::array<RenderPassAttachment, 2> m_outputs;
	};
}
