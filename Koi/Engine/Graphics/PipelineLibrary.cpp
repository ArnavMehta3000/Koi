#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>

namespace koi
{
	namespace
	{
		// Must match GBufferPass::CreateRenderTargets and the shader's SV_Target formats
		constexpr Format kGBufferFormats[] = { Format::R8G8B8A8_UNORM, Format::R16G16B16A16_UNORM, Format::R8G8B8A8_UNORM };
		constexpr Format kHDRFormat        = Format::R16G16B16A16_FLOAT;
		constexpr Format kDepthFormat      = Format::D32_FLOAT;
	}

	PipelineRecipe PipelineRecipe::GBuffer(std::string name, std::string vs, std::string ps)
	{
		PipelineRecipe recipe
		{
			.Name         = std::move(name),
			.VertexShader = std::move(vs),
			.PixelShader  = std::move(ps),
			.Desc         =
			{
				.Rasterizer        = { .CullMode = D3D12_CULL_MODE_BACK, .FrontCounterClockwise = true },
				.DepthStencil      = { .DepthEnable = true, .DepthWrite = true },
				.RenderTargetCount = static_cast<u32>(std::size(kGBufferFormats)),
				.DepthFormat       = kDepthFormat,
			},
		};

		std::ranges::copy(kGBufferFormats, recipe.Desc.RenderTargetFormats);
		return recipe;
	}

	PipelineRecipe PipelineRecipe::FullScreen(std::string name, std::string vs, std::string ps, Format target)
	{
		return PipelineRecipe
		{
			.Name         = std::move(name),
			.VertexShader = std::move(vs),
			.PixelShader  = std::move(ps),
			.Desc         =
			{
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
				.DepthStencil        = { .DepthEnable = false, .DepthWrite = false },
				.RenderTargetFormats = { target },
				.RenderTargetCount   = 1,
			},
		};
	}

	PipelineRecipe PipelineRecipe::Overlay(std::string name, std::string vs, std::string ps, Format target)
	{
		return PipelineRecipe
		{
			.Name         = std::move(name),
			.VertexShader = std::move(vs),
			.PixelShader  = std::move(ps),
			.Desc         =
			{
				.Rasterizer   = { .CullMode = D3D12_CULL_MODE_NONE },
				.BlendTargets =
				{
					{
						.BlendEnable = true,
						.SrcBlend    = D3D12_BLEND_SRC_ALPHA,
						.DstBlend    = D3D12_BLEND_INV_SRC_ALPHA,
					}
				},
				.DepthStencil        = { .DepthEnable = true, .DepthWrite = false, .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL },
				.RenderTargetFormats = { target },
				.RenderTargetCount   = 1,
				.DepthFormat         = kDepthFormat,
			},
		};
	}

	PipelineRecipe PipelineRecipe::DepthOnly(std::string name, std::string vs)
	{
		return PipelineRecipe
		{
			.Name         = std::move(name),
			.VertexShader = std::move(vs),
			.Desc         =
			{
				.Rasterizer        = { .CullMode = D3D12_CULL_MODE_BACK, .FrontCounterClockwise = true },
				.DepthStencil      = { .DepthEnable = true, .DepthWrite = true },
				.RenderTargetCount = 0,
				.DepthFormat       = kDepthFormat,
			},
		};
	}

	PipelineRecipe PipelineRecipe::Lines(std::string name, std::string vs, std::string ps, Format target)
	{
		return PipelineRecipe
		{
			.Name         = std::move(name),
			.VertexShader = std::move(vs),
			.PixelShader  = std::move(ps),
			.Desc         =
			{
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
				.DepthStencil        = { .DepthEnable = true, .DepthWrite = false, .DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL },
				.RenderTargetFormats = { target },
				.RenderTargetCount   = 1,
				.DepthFormat         = kDepthFormat,
				.TopologyType        = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
				.Topology            = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
			},
		};
	}

	PipelineRecipe& PipelineRecipe::Named(std::string name)
	{
		Name = std::move(name);
		return *this;
	}

	PipelineRecipe& PipelineRecipe::Cull(D3D12_CULL_MODE mode)
	{
		Desc.Rasterizer.CullMode = mode;
		return *this;
	}

	PipelineRecipe& PipelineRecipe::DepthBias(i32 bias, f32 slopeScaled)
	{
		Desc.Rasterizer.DepthBias            = bias;
		Desc.Rasterizer.SlopeScaledDepthBias = slopeScaled;
		return *this;
	}

	PipelineHandle PipelineLibrary::Register(GfxDevice& device, PipelineRecipe recipe)
	{
		Entry& entry = m_entries.emplace_back(Entry{ .Recipe = std::move(recipe) });
		if (!Build(device, entry))
		{
			KOI_LOG(Render, Error, "PipelineLibrary: failed to build '{}'", entry.Recipe.Name);
		}

		return entry.Handle;
	}

	u32 PipelineLibrary::RebuildAll(GfxDevice& device)
	{
		u32 rebuilt = 0;
		for (Entry& entry : m_entries)
		{
			if (Build(device, entry))
			{
				++rebuilt;
			}
			else
			{
				KOI_LOG(Render, Error, "PipelineLibrary: failed to rebuild '{}'", entry.Recipe.Name);
			}
		}

		KOI_LOG(Render, Debug, "PipelineLibrary: rebuilt {}/{} pipeline(s)", rebuilt, m_entries.size());
		return rebuilt;
	}

	u32 PipelineLibrary::RebuildUsing(GfxDevice& device, std::span<const std::string> shaderPaths)
	{
		auto uses = [shaderPaths](const std::string& path)
		{
			return !path.empty() && std::ranges::find(shaderPaths, path) != shaderPaths.end();
		};

		u32 rebuilt = 0;
		for (Entry& entry : m_entries)
		{
			if (!uses(entry.Recipe.VertexShader) && !uses(entry.Recipe.PixelShader))
				continue;

			if (Build(device, entry))
			{
				KOI_LOG(Render, Debug, "PipelineLibrary: rebuilt '{}'", entry.Recipe.Name);
				++rebuilt;
			}
			else
			{
				KOI_LOG(Render, Error, "PipelineLibrary: failed to rebuild '{}'", entry.Recipe.Name);
			}
		}

		KOI_LOG(Render, Info, "PipelineLibrary: rebuilt {} of {} pipeline(s)", rebuilt, m_entries.size());
		return rebuilt;
	}

	std::vector<std::string> PipelineLibrary::CollectShaderPaths() const
	{
		std::vector<std::string> paths;
		auto add = [&paths](const std::string& path)
		{
			if (!path.empty() && std::ranges::find(paths, path) == paths.end())
				paths.push_back(path);
		};

		for (const Entry& entry : m_entries)
		{
			add(entry.Recipe.VertexShader);
			add(entry.Recipe.PixelShader);
		}
		return paths;
	}

	void PipelineLibrary::Shutdown(GfxDevice& device)
	{
		for (Entry& entry : m_entries)
		{
			device.Destroy(entry.Handle);
		}
		m_entries.clear();
	}

	bool PipelineLibrary::Build(GfxDevice& device, Entry& entry)
	{
		ShaderCache& shaders         = ShaderCache::Get();
		const PipelineRecipe& recipe = entry.Recipe;

		PipelineDesc desc = recipe.Desc;
		desc.DebugName    = recipe.Name;

		if (!recipe.VertexShader.empty())
		{
			desc.VertexShader = shaders.GetShader(recipe.VertexShader);
			if (desc.VertexShader.empty()) return false;
		}

		if (!recipe.PixelShader.empty())
		{
			desc.PixelShader = shaders.GetShader(recipe.PixelShader);
			if (desc.PixelShader.empty()) return false;
		}

		if (entry.Handle)
			return device.RecreatePipeline(entry.Handle, desc);

		entry.Handle = device.CreatePipeline(desc);
		return static_cast<bool>(entry.Handle);
	}
}
