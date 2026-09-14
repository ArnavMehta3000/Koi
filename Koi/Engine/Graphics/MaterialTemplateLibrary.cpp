#include "Koi/Engine/Graphics/MaterialTemplateLibrary.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <algorithm>

namespace koi
{
	const MaterialTemplate& MaterialTemplateLibrary::Register(GfxDevice& device, PipelineLibrary& library, const PipelineRecipe& recipe, std::vector<MaterialParam> parameters)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		PipelineRecipe doubleSided = recipe;
		doubleSided.Named(recipe.Name + ".DoubleSided").Cull(D3D12_CULL_MODE_NONE);

		const MaterialTemplate& entry = *m_templates.emplace_back(std::make_unique<MaterialTemplate>(MaterialTemplate
		{
			.Name        = recipe.Name,
			.Parameters  = std::move(parameters),
			.SingleSided = library.Register(device, recipe),
			.DoubleSided = library.Register(device, std::move(doubleSided)),
		}));

		KOI_LOG_DEBUG("Material template '{}' registered with {} parameter(s)", entry.Name, entry.Parameters.size());
		return entry;
	}

	const MaterialTemplate* MaterialTemplateLibrary::Find(std::string_view name) const
	{
		const auto it = std::ranges::find_if(m_templates, [name](const auto& entry) { return entry->Name == name; });
		return it != m_templates.end() ? it->get() : nullptr;
	}

	const MaterialTemplate* MaterialTemplateLibrary::Default() const
	{
		return m_templates.empty() ? nullptr : m_templates.front().get();
	}
}
