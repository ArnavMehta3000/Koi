#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include <filesystem>
#include <memory>

namespace koi::editor
{
	namespace fs = std::filesystem;

	class MeshLoader
	{
	public:
		// Embedded images come back decoded when asked; external ones only as paths
		static std::unique_ptr<ModelAsset> Load(const fs::path& modelPath, bool decodeEmbeddedImages = false);
	};
}
