#pragma once
#include "Core/Types.h"
#include "Core/Logger.h"
#include <filesystem>
#include <vector>

namespace koi
{
	// Reads a compiled shader bytecode file (.cso) from the given filesystem.
	// Returns an empty vector and logs an error on failure.
	[[nodiscard]] inline std::vector<byte> LoadShaderBytecode(const std::filesystem::path& path)
	{
		std::vector<byte> result;
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (file.is_open())
		{
			result.resize(file.tellg());
			file.seekg(0, std::ios::beg);
			file.read(reinterpret_cast<char*>(result.data()), result.size());
			file.close();

			return result;
		}
		else
		{
			KOI_LOG_ERROR("Failed to load shader '{}'", path.string());
		}

		return {};
	}
}
