module;
#include <array>
#include <span>
#include <string_View>
export module Koi.Graphics.RenderPasses.ScenePass;

import Koi.Graphics.Types;
import Koi.Graphics.RenderPass;

export namespace koi
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
			PipelineHandle           Pipeline;
		};

		explicit ScenePass(const Desc& desc);

		std::string_view GetName()    const override { return "Scene"; }
		std::span<const RenderPassAttachment> GetOutputs() const override;
		void Execute(CommandList& cmd) override;

	private:
		Desc                                m_desc;
		std::array<RenderPassAttachment, 2> m_outputs;
	};
}
