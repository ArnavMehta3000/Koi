#pragma once
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include <memory>
#include <string>

namespace koi
{
	class GfxDevice;

	enum class MaterialParamType : u8 { Float, Float2, Float3, Float4, Texture };

	struct MaterialParam
	{
		std::string_view  Name;
		MaterialParamType Type;
		u32               Offset;  // byte offset into GPUMaterialData
	};

	// A shader plus what can be set on it. Not an asset: registered in C++, never browsable
	struct MaterialTemplate
	{
		std::string                Name;
		std::vector<MaterialParam> Parameters;

		// Two builds of one recipe. A material picks by its own double_sided flag
		PipelineHandle SingleSided;
		PipelineHandle DoubleSided;

		[[nodiscard]] inline PipelineHandle PipelineFor(bool doubleSided) const
		{
			return doubleSided ? DoubleSided : SingleSided;
		}
	};

	class MaterialTemplateLibrary : public NonCopyable
	{
	public:
		// Builds the recipe twice, once per cull mode, and keeps both handles
		const MaterialTemplate& Register(GfxDevice& device, PipelineLibrary& library, const PipelineRecipe& recipe, std::vector<MaterialParam> parameters);

		[[nodiscard]] const MaterialTemplate* Find(std::string_view name) const;
		[[nodiscard]] const MaterialTemplate* Default() const;  // Fallback material

	private:
		std::vector<std::unique_ptr<MaterialTemplate>> m_templates;
	};
}
