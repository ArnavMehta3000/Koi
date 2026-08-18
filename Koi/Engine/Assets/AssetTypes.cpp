#include "Koi/Engine/Assets/AssetTypes.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace koi
{
	const AssetTypeInfo* FindAssetType(std::string_view extension)
	{
		std::string ext(extension);
		std::ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		for (const AssetTypeInfo& type : kAssetTypes)
		{
			if (std::ranges::contains(type.Extensions, ext)) return &type;
		}
		return nullptr;
	}

	const AssetTypeInfo* FindAssetTypeByName(std::string_view name)
	{
		const auto it = std::ranges::find(kAssetTypes, name, &AssetTypeInfo::Name);
		return it != kAssetTypes.end() ? &*it : nullptr;
	}

	const AssetTypeInfo* FindAssetTypeByKind(AssetKind kind)
	{
		const auto it = std::ranges::find(kAssetTypes, kind, &AssetTypeInfo::Kind);
		return it != kAssetTypes.end() ? &*it : nullptr;
	}

	std::string_view ToString(AssetState state)
	{
		switch (state)
		{
		case AssetState::Unloaded: return "Unloaded";
		case AssetState::Loaded:   return "Loaded";
		case AssetState::Stale:    return "Stale";
		case AssetState::Missing:  return "Missing";
		case AssetState::Error:    return "Error";
		}
		return "Unknown";
	}
}
