#include "Koi/Engine/RHI/PipelineCache.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/RHI/Device.h"
#include "Koi/Engine/RHI/ShaderCache.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
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
		ForEach([this](const auto&, Entry& entry, const auto&)
		{
			if (entry.Handle)
			{
				m_device->Destroy(entry.Handle);
			}
		});
		m_raster.clear();
		m_compute.clear();
		m_paths.clear();
	}

	std::expected<BoundPipeline, PipelineError> PipelineCache::Get(const RasterPipeline& pipeline, const TargetFormats& targets)
	{
		KOI_ASSERT(pipeline.VertexShader.IsValid(), "A raster pipeline needs a vertex shader");

		auto it = m_raster.find(Key{ .Pipeline = pipeline, .Targets = targets });
		if (it == m_raster.end())
		{
			Key stored{ .Pipeline = pipeline, .Targets = targets };
			stored.Pipeline.VertexShader = Intern(pipeline.VertexShader);
			stored.Pipeline.PixelShader  = Intern(pipeline.PixelShader);

			it = m_raster.emplace(stored, Entry{}).first;
			Build(it->first, it->second);
		}
		return Result(it->second);
	}

	std::expected<BoundPipeline, PipelineError> PipelineCache::Get(const ComputePipeline& pipeline)
	{
		KOI_ASSERT(pipeline.ComputeShader.IsValid(), "A compute pipeline needs a compute shader");

		auto it = m_compute.find(pipeline);
		if (it == m_compute.end())
		{
			it = m_compute.emplace(ComputePipeline{ Intern(pipeline.ComputeShader) }, Entry{}).first;
			Build(it->first, it->second);
		}
		return Result(it->second);
	}

	std::expected<BoundPipeline, PipelineError> PipelineCache::Result(const Entry& entry)
	{
		if (entry.Handle)
			return BoundPipeline{ .Handle = entry.Handle, .Info = entry.Info };

		return std::unexpected(entry.Error.value_or(PipelineError::CreateFailed));
	}

	u32 PipelineCache::RebuildAll()
	{
		u32 rebuilt = 0;
		ForEach([&](const auto& key, Entry& entry, const auto&)
		{
			Build(key, entry);
			rebuilt += entry.Error ? 0 : 1;
		});

		KOI_LOG(Render, Info, "PipelineCache: rebuilt {}/{} pipeline(s)", rebuilt, GetCount());
		return rebuilt;
	}

	u32 PipelineCache::RebuildUsing(std::span<const std::string> shaderPaths)
	{
		const auto uses = [shaderPaths](ShaderRef shader)
		{
			return shader.IsValid() && std::ranges::find(shaderPaths, shader.Path) != shaderPaths.end();
		};

		u32 rebuilt = 0;
		ForEach([&](const auto& key, Entry& entry, const auto& shaders)
		{
			if (std::ranges::none_of(shaders, uses)) return;

			Build(key, entry);
			rebuilt += entry.Error ? 0 : 1;
		});

		KOI_LOG(Render, Info, "PipelineCache: rebuilt {} of {} pipeline(s)", rebuilt, GetCount());
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
		ShaderCache&          shaders  = ShaderCache::Get();
		entry.Name = DebugName(pipeline);

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
			.DebugName         = entry.Name,
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
		Report(entry);
	}

	void PipelineCache::Build(const ComputePipeline& pipeline, Entry& entry)
	{
		ZoneScoped;
		entry.Name = std::string(Stem(pipeline.ComputeShader.Path));

		const ComputePipelineDesc desc
		{
			.ComputeShader = ShaderCache::Get().GetShader(pipeline.ComputeShader.Path),
			.DebugName     = entry.Name,
		};

		entry.Error.reset();
		if (desc.ComputeShader.empty())
		{
			entry.Error = PipelineError::MissingShader;
		}
		else if (entry.Handle ? !m_device->RecreatePipeline(entry.Handle, desc) : !(entry.Handle = m_device->CreatePipeline(desc)))
		{
			entry.Error = PipelineError::CreateFailed;
		}
		else
		{
			entry.Info = ShaderCache::Get().GetInfo(pipeline.ComputeShader.Path);
		}
		Report(entry);
	}

	void PipelineCache::Report(const Entry& entry) const
	{
		if (!entry.Error) return;

		KOI_LOG(Render, Error, "PipelineCache: '{}' failed to build ({})", entry.Name,
			*entry.Error == PipelineError::MissingShader ? "shader missing or failed to compile" : "rejected by the driver");
	}
}
