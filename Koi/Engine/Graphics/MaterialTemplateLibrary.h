#pragma once
#include "Koi/Engine/Graphics/PipelineCache.h"
#include <memory>
#include <string>

namespace koi
{
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

		// A material picks by its own double_sided flag
		RasterPipeline SingleSided;
		RasterPipeline DoubleSided;

		[[nodiscard]] inline const RasterPipeline& PipelineFor(bool doubleSided) const
		{
			return doubleSided ? DoubleSided : SingleSided;
		}
	};

	class MaterialTemplateLibrary : public NonCopyable
	{
	public:
		// The double-sided variant is the same pipeline with culling off
		const MaterialTemplate& Register(std::string name, const RasterPipeline& pipeline, std::vector<MaterialParam> parameters);

		[[nodiscard]] const MaterialTemplate* Find(std::string_view name) const;
		[[nodiscard]] const MaterialTemplate* Default() const;  // Fallback material

	private:
		std::vector<std::unique_ptr<MaterialTemplate>> m_templates;
	};
}
