#pragma once
#include <filesystem>
#include <memory>

namespace koi
{
	struct ModelAsset;
}

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
