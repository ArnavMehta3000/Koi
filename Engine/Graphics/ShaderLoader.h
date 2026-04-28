#pragma once
#include "Core/Types.h"
#include "Core/Logger.h"
#include <Kirin/IO/VirtualFileSystem.h>
#include <filesystem>
#include <vector>

namespace koi
{
	// Reads a compiled shader bytecode file (.cso) from the given filesystem.
	// Returns an empty vector and logs an error on failure.
	inline std::vector<byte> LoadShaderBytecode(const krn::IFileSystem& fs, const std::filesystem::path& path)
	{
		auto result = fs.ReadFile(path);
		if (!result)
		{
			KOI_LOG_ERROR("Failed to load shader '{}': VFSError={}", path.string(), static_cast<int>(result.error()));
			return {};
		}

		const krn::IBlob* blob = result->get();
		const auto* data = static_cast<const byte*>(blob->Data());
		return std::vector<byte>(data, data + blob->Size());
	}
}
