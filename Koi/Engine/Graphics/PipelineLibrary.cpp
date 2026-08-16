#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>

namespace koi
{
	PipelineHandle PipelineLibrary::Register(GfxDevice& device, PipelineRecipe recipe)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		Entry& entry = m_entries.emplace_back(Entry{ .Recipe = std::move(recipe) });
		if (!Build(device, entry))
			KOI_LOG_ERROR("PipelineLibrary: failed to build '{}'", entry.Recipe.Name);

		return entry.Handle;
	}

	u32 PipelineLibrary::RebuildAll(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		u32 rebuilt = 0;
		for (Entry& entry : m_entries)
		{
			if (Build(device, entry))
				++rebuilt;
			else
				KOI_LOG_ERROR("PipelineLibrary: failed to rebuild '{}'", entry.Recipe.Name);
		}

		KOI_LOG_DEBUG("PipelineLibrary: rebuilt {}/{} pipeline(s)", rebuilt, m_entries.size());
		return rebuilt;
	}

	u32 PipelineLibrary::RebuildUsing(GfxDevice& device, std::span<const std::string> shaderPaths)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

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
				KOI_LOG_DEBUG("PipelineLibrary: rebuilt '{}'", entry.Recipe.Name);
				++rebuilt;
			}
			else
			{
				KOI_LOG_ERROR("PipelineLibrary: failed to rebuild '{}'", entry.Recipe.Name);
			}
		}

		KOI_LOG_INFO("PipelineLibrary: rebuilt {} of {} pipeline(s)", rebuilt, m_entries.size());
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
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		for (Entry& entry : m_entries)
		{
			device.Destroy(entry.Handle);
		}
		m_entries.clear();
	}

	bool PipelineLibrary::Build(GfxDevice& device, Entry& entry)
	{
		ShaderCache& shaders = ShaderCache::Get();
		const PipelineRecipe& recipe = entry.Recipe;

		PipelineDesc desc = recipe.Desc;
		desc.DebugName = recipe.Name;

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
