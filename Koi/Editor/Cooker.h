#pragma once
#include <filesystem>
#include <flat_map>
#include <string>

namespace koi::editor
{
	namespace fs = std::filesystem;

	struct CookRequest
	{
		using SceneMap = std::flat_map<std::string, std::string, std::less<>>;

		fs::path EngineSource;   // engine root (contains Content/)
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // project TOML in use
		fs::path OutputRoot;     // where the cooked layout is written
		SceneMap Scenes;         // [scenes.list], checked so a typo fails at cook time
	};

	// Compiles the shaders the registered pipelines use and copies content into the
	// <out>/Engine + <out>/Project layout the runtime VFS resolves against. Returns a status
	std::string Cook(const CookRequest& request);
}
