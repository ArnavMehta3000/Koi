#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/RHI/PipelineDesc.h"
#include <span>
#include <string>
#include <vector>

namespace koi
{
	class GfxDevice;

	// A pipeline described by the shaders it needs rather than the bytecode it was built from,
	// so it can be rebuilt whenever those shaders change
	struct PipelineRecipe
	{
		std::string  Name;          // debug name, owned here because PipelineDesc only holds a view
		std::string  VertexShader;  // virtual path without extension; empty when unused
		std::string  PixelShader;
		PipelineDesc Desc;          // shader bytecode and DebugName are filled in at build time
	};

	// Owns the recipe behind every pipeline the engine builds. Handles stay valid across a
	// rebuild (RecreatePipeline swaps the PSO in place), so anything holding one keeps working
	class PipelineLibrary : public NonCopyable
	{
	public:
		// Registers a recipe and builds it once. Invalid handle if the build fails
		[[nodiscard]] PipelineHandle Register(GfxDevice& device, PipelineRecipe recipe);

		// Returns how many rebuilt successfully. Call at a frame boundary
		u32 RebuildAll(GfxDevice& device);

		// Rebuilds only the pipelines built from one of these shader paths
		u32 RebuildUsing(GfxDevice& device, std::span<const std::string> shaderPaths);

		// Every distinct shader path across all registered recipes. What Cook compiles
		[[nodiscard]] std::vector<std::string> CollectShaderPaths() const;

		void Shutdown(GfxDevice& device);

	private:
		struct Entry
		{
			PipelineRecipe Recipe;
			PipelineHandle Handle;
		};

		// A failed build leaves any existing pipeline intact, so the frame still renders
		bool Build(GfxDevice& device, Entry& entry);

	private:
		std::vector<Entry> m_entries;
	};
}
