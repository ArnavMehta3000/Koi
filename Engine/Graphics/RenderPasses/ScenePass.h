#pragma once
#include "Graphics/RenderPasses/RenderPass.h"
#include <array>

namespace koi
{
	class  Scene;
	class  RenderWindow;
	struct Camera;
	struct RendererResources;

	class ScenePass : public RenderPass
	{
	public:
		struct Desc
		{
			Scene*                   Scene        = nullptr;
			const Camera*            Camera       = nullptr;
			const RendererResources* Resources    = nullptr;
			RenderWindow*            RenderWindow = nullptr;
			TextureHandle            DepthBuffer;
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
