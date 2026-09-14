#pragma once
#include "Koi/Engine/Assets/MaterialAsset.h"
#include <filesystem>

namespace koi::editor
{
	namespace fs = std::filesystem;

	// Writes a .mat.toml
	bool WriteMaterial(const fs::path& file, const MaterialSource& source);
}
