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

		KOI_REFLECT(RasterPipeline, VertexShader, PixelShader, Rasterizer, Blend, DepthStencil, Topology);
	};

	struct ComputePipeline
	{
		ShaderRef ComputeShader;

		bool operator==(const ComputePipeline&) const = default;

		KOI_REFLECT(ComputePipeline, ComputeShader);
	};

	struct TargetFormats
	{
		std::array<Format, MAX_RENDER_TARGETS> Colors{};
		u32                                    ColorCount = 0;
		Format                                 Depth      = Format::UNKNOWN;

		bool operator==(const TargetFormats&) const = default;

		KOI_REFLECT(TargetFormats, Colors, ColorCount, Depth);
	};

	enum class PipelineError : u8
	{
		MissingShader,  // not compiled, or failed to compile
		CreateFailed,   // the driver rejected it | D3D error
	};

	// A built pipeline plus its shader's reflection (compute only)
	struct BoundPipeline
	{
		PipelineHandle Handle;
		ShaderInfo     Info;
	};

	class PipelineCache : NonCopyable
	{
	public:
		void Init(GfxDevice& device);
		void Shutdown();

		// Builds on first use. A failure is remembered until a reload touches one of its shaders
		[[nodiscard]] std::expected<BoundPipeline, PipelineError> Get(const RasterPipeline& pipeline, const TargetFormats& targets);
		[[nodiscard]] std::expected<BoundPipeline, PipelineError> Get(const ComputePipeline& pipeline);

		// Call at a frame boundary with the GPU idle. Returns how many rebuilt
		u32 RebuildAll();
		u32 RebuildUsing(std::span<const std::string> shaderPaths);

		// Every distinct shader of every pipeline requested so far
		[[nodiscard]] std::vector<std::string> CollectShaderPaths() const;

		[[nodiscard]] inline size_t GetCount() const noexcept { return m_raster.size() + m_compute.size(); }

	private:
		struct Key
		{
			RasterPipeline Pipeline;
			TargetFormats  Targets;

			bool operator==(const Key&) const = default;

			KOI_REFLECT(Key, Pipeline, Targets);
		};

		struct Entry
		{
			PipelineHandle               Handle;
			ShaderInfo                   Info;
			std::string                  Name;
			std::optional<PipelineError> Error;
		};

		[[nodiscard]] ShaderRef Intern(ShaderRef shader);
		[[nodiscard]] static std::expected<BoundPipeline, PipelineError> Result(const Entry& entry);
		void Build(const Key& key, Entry& entry);
		void Build(const ComputePipeline& pipeline, Entry& entry);
		void Report(const Entry& entry) const;

		template <typename Fn>
		void ForEach(Fn&& fn)
		{
			for (auto& [key, entry] : m_raster)  fn(key, entry, std::array{ key.Pipeline.VertexShader, key.Pipeline.PixelShader });
			for (auto& [key, entry] : m_compute) fn(key, entry, std::array{ key.ComputeShader, ShaderRef() });
		}

	private:
		GfxDevice*                                                   m_device = nullptr;
		std::unordered_map<Key, Entry, utils::FieldHash>             m_raster;
		std::unordered_map<ComputePipeline, Entry, utils::FieldHash> m_compute;
		std::set<std::string, std::less<>>                           m_paths;  // what the stored keys' ShaderRefs view
	};
}
