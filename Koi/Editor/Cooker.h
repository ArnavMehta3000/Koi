#pragma once
#include <filesystem>
#include <string>

namespace koi::editor
{
	namespace fs = std::filesystem;

	class AssetDatabase;

	struct CookRequest
	{
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // explicit --project file
		fs::path OutputRoot;     // where the cooked layout is written
	};

	// Compiles the shaders the registered pipelines use and copies project content into
	// the <out>/Engine + <out>/Project layout the runtime VFS resolves against
	// Returns a one-line status suitable for the menu bar
	std::string Cook(const CookRequest& request, const AssetDatabase* database);
}
