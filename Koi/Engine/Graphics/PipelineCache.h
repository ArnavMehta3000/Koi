#pragma once
#include "Koi/Engine/Core/Hash.h"
#include "Koi/Engine/Graphics/RHI/PipelineDesc.h"
#include <array>
#include <expected>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace koi
{
	class GfxDevice;

	// A shader by virtual path (w/o ext)
	struct ShaderRef
	{
		std::string_view Path;
		u64              Id = 0;

		constexpr ShaderRef() = default;
		constexpr ShaderRef(std::string_view path) : Path(path), Id(path.empty() ? 0 : utils::Fnv1a(path)) {}
		constexpr ShaderRef(const char* path) : ShaderRef(std::string_view(path)) {}

		[[nodiscard]] constexpr bool IsValid() const noexcept { return Id != 0; }
		[[nodiscard]] constexpr u64 GetHash() const noexcept { return Id; }
		constexpr bool operator==(const ShaderRef& other) const noexcept { return Id == other.Id; }
	};

	// Shaders and fixed-function state. The formats come from whichever pass binds it
	struct RasterPipeline
	{
		ShaderRef              VertexShader;
		ShaderRef              PixelShader;  // none for depth-only
		RasterizerDesc         Rasterizer   = RasterizerDesc::CullBack();
		BlendTargetDesc        Blend        = BlendTargetDesc::Opaque();  // applied to every target
		DepthStencilDesc       DepthStencil = DepthStencilDesc::Default();
		D3D_PRIMITIVE_TOPOLOGY Topology     = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		bool operator==(const RasterPipeline&) const = default;
	};

	struct TargetFormats
	{
		std::array<Format, MAX_RENDER_TARGETS> Colors{};
		u32                                    ColorCount = 0;
		Format                                 Depth      = Format::UNKNOWN;

		bool operator==(const TargetFormats&) const = default;
	};

	enum class PipelineError : u8
	{
		MissingShader,  // not compiled, or failed to compile
		CreateFailed,   // the driver rejected it | D3D error
	};

	class PipelineCache : NonCopyable
	{
	public:
		void Init(GfxDevice& device);
		void Shutdown();

		[[nodiscard]] std::expected<PipelineHandle, PipelineError> Get(const RasterPipeline& pipeline, const TargetFormats& targets);

		// Call at a frame boundary with the GPU idle. Returns how many rebuilt
		u32 RebuildAll();
		u32 RebuildUsing(std::span<const std::string> shaderPaths);

		// Every distinct shader of every pipeline requested so far
		[[nodiscard]] std::vector<std::string> CollectShaderPaths() const;

		[[nodiscard]] inline size_t GetCount() const noexcept { return m_entries.size(); }

	private:
		struct Key
		{
			RasterPipeline Pipeline;
			TargetFormats  Targets;

			bool operator==(const Key&) const = default;
		};

		struct Entry
		{
			PipelineHandle               Handle;
			std::optional<PipelineError> Error;
		};

		[[nodiscard]] ShaderRef Intern(ShaderRef shader);
		void Build(const Key& key, Entry& entry);

	private:
		GfxDevice*                                       m_device = nullptr;
		std::unordered_map<Key, Entry, utils::FieldHash> m_entries;
		std::set<std::string, std::less<>>               m_paths;  // what the stored keys' ShaderRefs view
	};
}
