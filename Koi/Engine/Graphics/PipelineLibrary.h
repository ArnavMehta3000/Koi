#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/RHI/PipelineDesc.h"
#include <span>
#include <string>
#include <vector>

namespace koi
{
	class GfxDevice;

	struct PipelineRecipe
	{
		std::string  Name;
		std::string  VertexShader;  // virtual path without extension | empty when unused
		std::string  PixelShader;
		PipelineDesc Desc;          // shader bytecode and DebugName are filled in at build time

		[[nodiscard]] static PipelineRecipe GBuffer   (std::string name, std::string vs, std::string ps);
		[[nodiscard]] static PipelineRecipe FullScreen(std::string name, std::string vs, std::string ps, Format target);
		[[nodiscard]] static PipelineRecipe DepthOnly (std::string name, std::string vs);

		PipelineRecipe& Named(std::string name);
		PipelineRecipe& Cull(D3D12_CULL_MODE mode);
		PipelineRecipe& DepthBias(i32 bias, f32 slopeScaled);
	};

	// Owns the recipe behind every pipeline the engine builds. Handles stay valid across a rebuild
	// See Device::RecreatePipeline()
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

		// A failed build leaves any existing pipeline intact
		bool Build(GfxDevice& device, Entry& entry);

	private:
		std::vector<Entry> m_entries;
	};
}
