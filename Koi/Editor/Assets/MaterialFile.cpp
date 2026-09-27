#include "Koi/Editor/Assets/MaterialFile.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Serializer.h"
#include <toml++/toml.hpp>
#include <fstream>

namespace koi::editor
{
	bool WriteMaterial(const fs::path& file, const MaterialSource& source)
	{
		const ImportMaterial::Constants& c = source.Constants;

		toml::table constants;
		constants.insert("base_color", toml::array{ c.BaseColorFactor.x, c.BaseColorFactor.y, c.BaseColorFactor.z, c.BaseColorFactor.w });
		constants.insert("emissive",   toml::array{ c.EmissiveFactor.x, c.EmissiveFactor.y, c.EmissiveFactor.z });
		constants.insert("metallic",           c.MetallicFactor);
		constants.insert("roughness",          c.RoughnessFactor);
		constants.insert("normal_scale",       c.NormalScale);
		constants.insert("occlusion_strength", c.OcclusionStrength);
		constants.insert("alpha_cutoff",       c.AlphaCutoff);

		toml::table textures;
		for (size_t i = 0; i < kMaterialTextureCount; ++i)
		{
			if (source.Textures[i].IsValid())
				textures.insert(ToString(static_cast<MaterialTextureSlot>(i)), source.Textures[i].ToString());
		}

		const std::string_view mode =
			  source.Mode == ImportMaterial::AlphaMode::Mask  ? "mask"
			: source.Mode == ImportMaterial::AlphaMode::Blend ? "blend"
			:                                                   "opaque";

		toml::table root;
		root.insert("name",         source.Name);
		root.insert("alpha_mode",   mode);
		root.insert("double_sided", source.DoubleSided);
		if (!source.Template.empty()) root.insert("template", source.Template);
		root.insert("constants", std::move(constants));
		root.insert("textures",  std::move(textures));

		std::ofstream out(file, std::ios::trunc);
		if (!out)
		{
			KOI_LOG_ERROR("Cannot write material {}", file.string());
			return false;
		}

		out << TomlDocument(root) << '\n';
		return true;
	}
}
