#pragma once
#include "Graphics/Types.h"
#include <filesystem>
#include <memory>

namespace koi
{
	namespace fs = std::filesystem;

	class MeshLoader
	{
	public:
		static std::unique_ptr<ModelAsset> Load(const fs::path& modelPath);
	};
}
