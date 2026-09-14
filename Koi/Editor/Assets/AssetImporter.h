#pragma once
#include <string_view>

namespace koi::editor
{
	class AssetDatabase;

	// Expands GLTF file embedded data into raw png + material TOML files
	bool ImportMesh(AssetDatabase& database, std::string_view meshPath);
}
