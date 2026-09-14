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
		Unloaded = 0,  // known to the database, not resident
		Loaded,        // resident in an AssetCache
		Stale,         // resident, but the source changed on disk since it was loaded
		Missing,       // has a manifest entry, no file behind it
		Error,         // load or compile failed; see the log
	};

	// Slot -> material bindings, written beside a mesh under its own name
	inline constexpr std::string_view kMeshSidecarExt = ".mesh.toml";

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

		inline constexpr std::array kMeshExt    = { ".gltf"sv, ".glb"sv };
		inline constexpr std::array kTextureExt = { ".png"sv, ".jpg"sv, ".jpeg"sv, ".dds"sv };
		inline constexpr std::array kShaderExt  = { ".hlsl"sv };
		inline constexpr std::array kSceneExt    = { ".scene.toml"sv };
		inline constexpr std::array kMaterialExt = { ".mat.toml"sv };
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
