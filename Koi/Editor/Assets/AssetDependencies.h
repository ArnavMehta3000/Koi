#pragma once
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include <filesystem>
#include <vector>

namespace koi::editor
{
	namespace fs = std::filesystem;

	[[nodiscard]] std::vector<AssetKey> ScanDependencies(AssetKind kind, const fs::path& diskPath, const fs::path& projectSource);
}
