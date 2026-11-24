#pragma once
#include "Koi/Engine/Core/Error.h"
#include "Koi/Engine/RHI/Types.h"
#include <filesystem>

namespace koi
{
	class GfxDevice;
}

namespace koi::utils
{
	namespace fs = std::filesystem;

	// Call after the frame's commands are submitted; stalls until the PNG is written
	[[nodiscard]] Result<void> SaveScreenshot(GfxDevice& device, TextureHandle texture, const fs::path& path);

	// <dir>/<local date and time>.png
	[[nodiscard]] fs::path TimestampedScreenshotPath(const fs::path& dir);
}
