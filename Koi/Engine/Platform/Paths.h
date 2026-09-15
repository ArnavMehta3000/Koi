#pragma once
#include <filesystem>

namespace koi::Paths
{
	namespace fs = std::filesystem;

	// Directory containing the running executable
	fs::path ExecutableDir();
}
