#pragma once
#include "Graphics/Types.h"
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

	// Base class for a discrete rendering step.
	// Subclasses declare their inputs/outputs for graph validation and override Execute.
	class RenderPass
	{
	public:
		virtual ~RenderPass() = default;

		virtual std::string_view GetName() const = 0;

		// Textures this pass reads (must be produced by an earlier pass or externally).
		virtual std::span<const RenderPassAttachment> GetInputs()  const { return {}; }

		// Textures this pass writes to (produces for downstream passes).
		virtual std::span<const RenderPassAttachment> GetOutputs() const { return {}; }

		virtual void Execute(CommandList& cmd) = 0;
	};

	// Ordered list of render passes. Compile() validates input/output connectivity;
	// Execute() runs all passes in declaration order.
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

		// Validates that every declared input was produced by a prior pass.
		// Inputs not covered by any prior pass are assumed externally produced and logged at trace level.
		void Compile();

		// Calls Compile() if the graph has been modified, then executes all passes in order.
		void Execute(CommandList& cmd);

		void Clear();

	private:
		std::vector<std::unique_ptr<RenderPass>> m_passes;
		bool m_dirty = true;
	};
}
