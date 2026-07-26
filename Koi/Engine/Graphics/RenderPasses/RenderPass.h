#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <span>
#include <vector>
#include <memory>
#include <string_view>

namespace koi
{
	class CommandList;

	// Declares a single texture this pass reads from or writes to.
	struct RenderPassAttachment
	{
		TextureHandle    Texture;
		std::string_view Name;
	};

	class RenderPass
	{
	public:
		virtual ~RenderPass() = default;

		virtual std::string_view GetName() const = 0;
		virtual std::span<const RenderPassAttachment> GetInputs()  const { return {}; }
		virtual std::span<const RenderPassAttachment> GetOutputs() const { return {}; }
		virtual void Reset() {}
		virtual void Execute(CommandList& cmd) = 0;
		virtual void OnResize([[maybe_unused]] u32 w, [[maybe_unused]] u32 h) {}
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

		template<typename T>
		T* GetPass()
		{
			for (auto& p : m_passes)
			{
				if (T* t = dynamic_cast<T*>(p.get()))
					return t;
			}
			return nullptr;
		}

		// Validates that every declared input name is produced as an output by a prior pass
		// Inputs not covered by any prior pass are assumed externally produced and logged at trace level
		void Compile();
		void Execute(CommandList& cmd);
		void ResetAll();
		void OnResize(u32 w, u32 h);

		void Clear();

	private:
		std::vector<std::unique_ptr<RenderPass>> m_passes;
		bool m_dirty = true;
	};
}
