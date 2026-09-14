#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <memory>
#include <span>
#include <vector>

namespace koi
{
	class CommandList;

	// A texture a pass produced
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
		virtual std::span<const RenderPassAttachment> GetOutputs() const { return {}; }
		virtual void Reset() {}
		virtual void Execute(CommandList& cmd) = 0;
	};

	class PassList
	{
	public:
		template<typename T, typename... Args>
		T& Add(Args&&... args)
		{
			auto& p = m_passes.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
			return *static_cast<T*>(p.get());
		}

		template<typename T>
		T* Find()
		{
			for (auto& p : m_passes)
			{
				if (T* t = dynamic_cast<T*>(p.get()))
					return t;
			}
			return nullptr;
		}

		[[nodiscard]] inline std::span<const std::unique_ptr<RenderPass>> GetPasses() const { return m_passes; }

		void Execute(CommandList& cmd);
		void ResetAll();
		void Clear();

	private:
		std::vector<std::unique_ptr<RenderPass>> m_passes;
	};
}
