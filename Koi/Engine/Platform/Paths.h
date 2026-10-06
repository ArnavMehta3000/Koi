#pragma once
#include <filesystem>

namespace koi::utils
{
	namespace fs = std::filesystem;

	// Directory containing the running executable
	fs::path ExecutableDir();
}
