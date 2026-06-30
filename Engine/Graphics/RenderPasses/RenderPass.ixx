module;
#include <span>
#include <vector>
#include <memory>
#include <string_view>
export module Koi.Graphics.RenderPass;

import Koi.Core.Logger;
import Koi.Graphics.Types;

export namespace koi
{
	class CommandList;

	struct RenderPassAttachment
	{
		TextureHandle    Texture;
		std::string_view Name;
	};

	class RenderPass
	{
	public:
		virtual ~RenderPass() = default;

		virtual std::string_view GetName()    const = 0;
		virtual std::span<const RenderPassAttachment> GetInputs()  const { return {}; }
		virtual std::span<const RenderPassAttachment> GetOutputs() const { return {}; }
		virtual void Execute(CommandList& cmd) = 0;
	};

	class RenderGraph
	{
	public:
		template<typename T, typename... Args>
		T& AddPass(Args&&... args)
		{
			m_dirty = true;
			auto& p = m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			return *static_cast<T*>(p.get());
		}

		void Compile();
		void Execute(CommandList& cmd);
		void Clear();

	private:
		std::vector<std::unique_ptr<RenderPass>> m_passes;
		bool m_dirty = true;
	};
}
