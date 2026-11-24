#pragma once
#include "Koi/Engine/RHI/PipelineCache.h"
#include <array>
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

		// Indexed [masked][double sided]
		std::array<std::array<RasterPipeline, 2>, 2> Pipelines;

		[[nodiscard]] inline const RasterPipeline& PipelineFor(bool masked, bool doubleSided) const
		{
			return Pipelines[masked][doubleSided];
		}
	};

	class MaterialTemplateLibrary : public NonCopyable
	{
	public:
		// Double-sided variants turn culling off, masked ones swap in maskedPixelShader (if there is one)
		const MaterialTemplate& Register(std::string name, const RasterPipeline& pipeline, ShaderRef maskedPixelShader, std::vector<MaterialParam> parameters);

		[[nodiscard]] const MaterialTemplate* Find(std::string_view name) const;
		[[nodiscard]] const MaterialTemplate* Default() const;  // Fallback material

	private:
		std::vector<std::unique_ptr<MaterialTemplate>> m_templates;
	};
}
