#pragma once
#include "Koi/Engine/Core/Types.h"
#include <array>
#include <span>

namespace koi
{
	enum class AssetKind : u8
	{
		Unknown = 0,
		Mesh,
		Texture,
		Shader,
		Scene,
		Material,
	};

	enum class AssetState : u8
	{
		Unloaded = 0,  // on disk, not resident
		Loaded,        // resident in an AssetCache
	};

	// ImGui drag-drop payload id for an asset dragged out of the Assets panel
	inline constexpr const char* kAssetDragDropType = "KOI_ASSET";

	struct AssetTypeInfo
	{
		AssetKind                         Kind;
		std::string_view                  Name;        // display + manifest "type" field
		std::span<const std::string_view> Extensions;  // lowercase, leading dot
	};

	namespace internal
	{
		using namespace std::string_view_literals;  // For sv

		inline constexpr std::array kMeshExt    = { ".kmesh"sv };
		inline constexpr std::array kTextureExt = { ".ktex"sv };
		inline constexpr std::array kShaderExt  = { ".hlsl"sv };
		inline constexpr std::array kSceneExt    = { ".scene.toml"sv };
		inline constexpr std::array kMaterialExt = { ".kmat"sv };
	}

	inline constexpr std::array kAssetTypes = std::to_array<AssetTypeInfo>(
	{
		{ AssetKind::Mesh,     "mesh",     internal::kMeshExt     },
		{ AssetKind::Texture,  "texture",  internal::kTextureExt  },
		{ AssetKind::Shader,   "shader",   internal::kShaderExt   },
		{ AssetKind::Scene,    "scene",    internal::kSceneExt    },
		{ AssetKind::Material, "material", internal::kMaterialExt },
	});

	[[nodiscard]] const AssetTypeInfo* FindAssetTypeForFile(std::string_view fileName);
	[[nodiscard]] const AssetTypeInfo* FindAssetTypeByName(std::string_view name);
	[[nodiscard]] const AssetTypeInfo* FindAssetTypeByKind(AssetKind kind);
	[[nodiscard]] std::string_view AssetExtensionOf(std::string_view fileName);
	[[nodiscard]] std::string_view ToString(AssetState state);
}
