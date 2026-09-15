#pragma once
#include <filesystem>
#include <string>

namespace koi::editor
{
	namespace fs = std::filesystem;

	struct CookRequest
	{
		fs::path EngineSource;   // engine root (contains Content/)
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // project TOML in use
		fs::path OutputRoot;     // where the cooked layout is written
	};

	// Compiles the shaders the registered pipelines use and copies content into the
	// <out>/Engine + <out>/Project layout the runtime VFS resolves against. Returns a status
	std::string Cook(const CookRequest& request);
}
