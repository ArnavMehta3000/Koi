#pragma once
#include "Koi/Engine/Assets/MaterialAsset.h"
#include "Koi/Engine/Core/UUID.h"
#include <filesystem>

namespace koi::editor
{
	namespace fs = std::filesystem;

	// Writes a .kmat, keeping the id of one already there
	bool WriteMaterial(const fs::path& file, const MaterialSource& source);

	// Texture keys become ids, so every texture has to be an imported .ktex
	bool WriteMaterialFile(const fs::path& file, const UUID& id, const MaterialSource& source);
}
