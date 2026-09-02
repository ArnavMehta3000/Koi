#include "Koi/Engine/Assets/AssetTypes.h"
#include <algorithm>
#include <cctype>
#include <string>

namespace koi
{
	namespace
	{
		// Longest match wins, so a ".scene.toml" is a scene and not whatever else claims ".toml"
		size_t MatchExtension(std::string_view fileName, const AssetTypeInfo** outType)
		{
			std::string lower(fileName);
			std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

			size_t best = 0;
			for (const AssetTypeInfo& type : kAssetTypes)
			{
				for (const std::string_view ext : type.Extensions)
				{
					if (ext.size() <= best || !std::string_view(lower).ends_with(ext)) continue;

					best = ext.size();
					if (outType) *outType = &type;
				}
			}
			return best;
		}
	}

	const AssetTypeInfo* FindAssetTypeForFile(std::string_view fileName)
	{
		const AssetTypeInfo* type = nullptr;
		return MatchExtension(fileName, &type) > 0 ? type : nullptr;
	}

	std::string_view AssetExtensionOf(std::string_view fileName)
	{
		const size_t length = MatchExtension(fileName, nullptr);
		return length > 0 ? fileName.substr(fileName.size() - length) : std::string_view();
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
