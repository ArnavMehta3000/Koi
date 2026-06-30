module;
#include <filesystem>
#include <memory>
export module Koi.Graphics.MeshLoader;

import Koi.Graphics.Types;

export namespace koi
{
	namespace fs = std::filesystem;

	class MeshLoader
	{
	public:
		static std::unique_ptr<ModelAsset> Load(const fs::path& modelPath);
	};
}
