#pragma once
#include "Koi/Engine/Graphics/RenderGraph/RenderGraph.h"
#include "Koi/Engine/Graphics/RenderPasses/PassData.h"
#include <memory>
#include <vector>

namespace koi
{
	// Owns what outlives a frame (upload buffers, settings); records into the graph each frame
	class RenderPass
	{
	public:
		virtual ~RenderPass() = default;
		virtual std::string_view GetName() const = 0;
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

		inline void Clear() { m_passes.clear(); }

	private:
		std::vector<std::unique_ptr<RenderPass>> m_passes;
	};
}
