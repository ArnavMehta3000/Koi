#pragma once
#include "Koi/Engine/Assets/AssetData.h"
#include "Koi/Engine/Assets/AssetKey.h"
#include <array>
#include <string>
#include <string_view>

namespace koi
{
	// The texture channels a material can bind (ordered by GPU write order)
	enum class MaterialTextureSlot : u8
	{
		BaseColor = 0,
		MetallicRoughness,
		Normal,
		Occlusion,
		Emissive,
		COUNT
	};

	inline constexpr size_t kMaterialTextureCount = static_cast<size_t>(MaterialTextureSlot::COUNT);

	// What a .kmat decodes to
	struct MaterialSource
	{
		std::string Name;
		std::string Template;  // empty picks the library's default
		ImportMaterial::Constants Constants{};
		ImportMaterial::AlphaMode Mode = ImportMaterial::AlphaMode::Opaque;
		bool DoubleSided = false;
		std::array<AssetKey, kMaterialTextureCount> Textures{};

		// The key a slot's texture parameter is stored under
		[[nodiscard]] static constexpr std::string_view SlotName(MaterialTextureSlot slot)
		{
			constexpr std::array<std::string_view, kMaterialTextureCount> kKeys =
			{
				"base_color", "metallic_roughness", "normal", "occlusion", "emissive"
			};
			const size_t index = static_cast<size_t>(slot);
			return index < kKeys.size() ? kKeys[index] : std::string_view("unknown");
		}
	};
}
