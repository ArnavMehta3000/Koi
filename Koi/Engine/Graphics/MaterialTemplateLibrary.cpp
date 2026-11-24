#include "Koi/Engine/Graphics/MaterialTemplateLibrary.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>

namespace koi
{
	const MaterialTemplate& MaterialTemplateLibrary::Register(std::string name, const RasterPipeline& pipeline, ShaderRef maskedPixelShader, std::vector<MaterialParam> parameters)
	{
		RasterPipeline masked = pipeline;
		if (maskedPixelShader.IsValid())
		{
			masked.PixelShader = maskedPixelShader;
		}

		const auto withDoubleSided = [](const RasterPipeline& single)
		{
			RasterPipeline both = single;
			both.Rasterizer.CullMode = D3D12_CULL_MODE_NONE;
			return std::array{ single, both };
		};

		const MaterialTemplate& entry = *m_templates.emplace_back(std::make_unique<MaterialTemplate>(MaterialTemplate
		{
			.Name        = std::move(name),
			.Parameters  = std::move(parameters),
			.Pipelines   = { withDoubleSided(pipeline), withDoubleSided(masked) },
		}));

		KOI_LOG(Render, Trace, "Material template '{}' registered with {} parameter(s)", entry.Name, entry.Parameters.size());
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
