#include "Koi/Engine/Graphics/PipelineCache.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cassert>
#include <format>

namespace koi
{
	namespace
	{
		constexpr D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyTypeOf(D3D_PRIMITIVE_TOPOLOGY topology)
		{
			switch (topology)
			{
				case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
				case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
				case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
				default:                               return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			}
		}

		std::string_view Stem(std::string_view path)
		{
			const size_t slash = path.find_last_of('/');
			return slash == std::string_view::npos ? path : path.substr(slash + 1);
		}

		std::string DebugName(const RasterPipeline& pipeline)
		{
			return pipeline.PixelShader.IsValid()
				? std::format("{}+{}", Stem(pipeline.VertexShader.Path), Stem(pipeline.PixelShader.Path))
				: std::string(Stem(pipeline.VertexShader.Path));
		}
	}

	void PipelineCache::Init(GfxDevice& device)
	{
		m_device = &device;
	}

	void PipelineCache::Shutdown()
	{
		for (auto& [key, entry] : m_entries)
		{
			if (entry.Handle)
			{
				m_device->Destroy(entry.Handle);
			}
		}
		m_entries.clear();
		m_paths.clear();
	}

	std::expected<PipelineHandle, PipelineError> PipelineCache::Get(const RasterPipeline& pipeline, const TargetFormats& targets)
	{
		assert(pipeline.VertexShader.IsValid() && "A raster pipeline needs a vertex shader");

		auto it = m_entries.find(Key{ .Pipeline = pipeline, .Targets = targets });
		if (it == m_entries.end())
		{
			Key stored{ .Pipeline = pipeline, .Targets = targets };
			stored.Pipeline.VertexShader = Intern(pipeline.VertexShader);
			stored.Pipeline.PixelShader  = Intern(pipeline.PixelShader);

			it = m_entries.emplace(stored, Entry{}).first;
			Build(it->first, it->second);
		}

		if (it->second.Handle)
			return it->second.Handle;

		return std::unexpected(it->second.Error.value_or(PipelineError::CreateFailed));
	}

	u32 PipelineCache::RebuildAll()
	{
		u32 rebuilt = 0;
		for (auto& [key, entry] : m_entries)
		{
			Build(key, entry);
			rebuilt += entry.Error ? 0 : 1;
		}

		KOI_LOG(Render, Info, "PipelineCache: rebuilt {}/{} pipeline(s)", rebuilt, m_entries.size());
		return rebuilt;
	}

	u32 PipelineCache::RebuildUsing(std::span<const std::string> shaderPaths)
	{
		const auto uses = [shaderPaths](ShaderRef shader)
		{
			return shader.IsValid() && std::ranges::find(shaderPaths, shader.Path) != shaderPaths.end();
		};

		u32 rebuilt = 0;
		for (auto& [key, entry] : m_entries)
		{
			if (!uses(key.Pipeline.VertexShader) && !uses(key.Pipeline.PixelShader))
				continue;

			Build(key, entry);
			rebuilt += entry.Error ? 0 : 1;
		}

		KOI_LOG(Render, Info, "PipelineCache: rebuilt {} of {} pipeline(s)", rebuilt, m_entries.size());
		return rebuilt;
	}

	std::vector<std::string> PipelineCache::CollectShaderPaths() const
	{
		return { m_paths.begin(), m_paths.end() };
	}

	ShaderRef PipelineCache::Intern(ShaderRef shader)
	{
		if (!shader.IsValid()) return {};
		return ShaderRef(*m_paths.emplace(shader.Path).first);
	}

	void PipelineCache::Build(const Key& key, Entry& entry)
	{
		ZoneScoped;
		const RasterPipeline& pipeline = key.Pipeline;
		const std::string     name     = DebugName(pipeline);
		ShaderCache&          shaders  = ShaderCache::Get();

		PipelineDesc desc
		{
			.VertexShader      = shaders.GetShader(pipeline.VertexShader.Path),
			.PixelShader       = pipeline.PixelShader.IsValid() ? shaders.GetShader(pipeline.PixelShader.Path) : std::span<const byte>(),
			.Rasterizer        = pipeline.Rasterizer,
			.DepthStencil      = pipeline.DepthStencil,
			.RenderTargetCount = key.Targets.ColorCount,
			.DepthFormat       = key.Targets.Depth,
			.TopologyType      = TopologyTypeOf(pipeline.Topology),
			.Topology          = pipeline.Topology,
			.DebugName         = name,
		};
		std::ranges::copy(key.Targets.Colors, desc.RenderTargetFormats);
		std::ranges::fill(desc.BlendTargets, pipeline.Blend);

		// A failed rebuild keeps the old pipeline drawing
		entry.Error.reset();
		if (desc.VertexShader.empty() || (pipeline.PixelShader.IsValid() && desc.PixelShader.empty()))
		{
			entry.Error = PipelineError::MissingShader;
		}
		else if (entry.Handle ? !m_device->RecreatePipeline(entry.Handle, desc) : !(entry.Handle = m_device->CreatePipeline(desc)))
		{
			entry.Error = PipelineError::CreateFailed;
		}

		if (entry.Error)
		{
			KOI_LOG(Render, Error, "PipelineCache: '{}' failed to build ({})", name,
				*entry.Error == PipelineError::MissingShader ? "shader missing or failed to compile" : "rejected by the driver");
		}
	}
}
