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
		Script,
		Scene,
	};

	enum class AssetState : u8
	{
		Unloaded = 0,  // known to the database, not resident
		Loaded,        // resident in an AssetCache
		Stale,         // resident, but the source changed on disk since it was loaded
		Missing,       // has a manifest entry, no file behind it
		Error,         // load or compile failed; see the log
	};

	// ImGui drag-drop payload id for an asset dragged out of the Assets panel
	inline constexpr const char* kAssetDragDropType = "KOI_ASSET";

	struct AssetTypeInfo
	{
		AssetKind                         Kind;
		std::string_view                  Name;        // display + manifest "type" field
		std::span<const std::string_view> Extensions;  // lowercase, leading dot
		bool                              TextEditable = false;  // opens in the Text Editor panel
	};

	namespace internal
	{
		inline constexpr std::array kMeshExt    = { std::string_view(".gltf"), std::string_view(".glb") };
		inline constexpr std::array kTextureExt = { std::string_view(".png"), std::string_view(".jpg"), std::string_view(".jpeg"), std::string_view(".dds") };
		inline constexpr std::array kShaderExt  = { std::string_view(".hlsl") };
		inline constexpr std::array kScriptExt  = { std::string_view(".as") };
		inline constexpr std::array kSceneExt   = { std::string_view(".toml") };
	}

	inline constexpr std::array kAssetTypes = std::to_array<AssetTypeInfo>(
	{
		{ AssetKind::Mesh,    "mesh",    internal::kMeshExt                 },
		{ AssetKind::Texture, "texture", internal::kTextureExt              },
		{ AssetKind::Shader,  "shader",  internal::kShaderExt,  true        },
		{ AssetKind::Script,  "script",  internal::kScriptExt,  true        },
		{ AssetKind::Scene,   "scene",   internal::kSceneExt,   true        },
	});

	// Null for anything unrecognised - that is what keeps AssetCache.h / MeshAsset.cpp out
	// of the browser without an allowlist living in the panel
	[[nodiscard]] const AssetTypeInfo* FindAssetType(std::string_view extension);
	[[nodiscard]] const AssetTypeInfo* FindAssetTypeByName(std::string_view name);
	[[nodiscard]] const AssetTypeInfo* FindAssetTypeByKind(AssetKind kind);

	[[nodiscard]] std::string_view ToString(AssetState state);
}
