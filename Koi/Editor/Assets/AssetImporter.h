#pragma once
#include "Koi/Engine/Core/UUID.h"

namespace koi::editor
{
	class AssetDatabase;

	// Expands GLTF file embedded data into raw png + material TOML files
	bool ImportMesh(AssetDatabase& database, const UUID& meshId);
}
