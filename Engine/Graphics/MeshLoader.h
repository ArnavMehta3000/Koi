#pragma once
#include "Graphics/Types.h"
#include <filesystem>
#include <vector>
#include <optional>

namespace koi
{
	namespace fs = std::filesystem;

	class MeshLoader
	{
	public:
		static std::optional<ModelAsset> Load(const fs::path& path);
	};
}
