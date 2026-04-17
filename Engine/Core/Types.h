#pragma once
#include "Kirin/Common/StandardTypes.h"
#include <string_view>

namespace koi
{
	// Pull krn standard types into koi namespace
	using namespace krn::types;

    struct NonCopyable
    {
        NonCopyable() = default;
        ~NonCopyable() = default;

        NonCopyable(const NonCopyable&) = delete;
        NonCopyable& operator=(const NonCopyable&) = delete;
    };

    struct NonMovable
    {
        NonMovable() = default;
        ~NonMovable() = default;

        NonMovable(NonMovable&&) = delete;
        NonMovable& operator=(NonMovable&&) = delete;
    };

    struct NonCopyableNonMovable : NonCopyable, NonMovable
    {
        NonCopyableNonMovable() = default;
    };


	
	/// No better place to put this
	constexpr std::string_view ExtractFilename(std::string_view path) noexcept
	{
		// Find the last occurrence of either '/' or '\'
		size_t lastSlash = path.find_last_of("/\\");

		// No slash found, return the entire string (it's already just a filename)
		if (lastSlash == std::string_view::npos)
		{
			return path;
		}

		// Return substring starting after the last slash
		return path.substr(lastSlash + 1);
	}

	constexpr std::string_view ExtractFilenameNoExt(std::string_view path) noexcept
	{
		std::string_view filename = ExtractFilename(path);

		// Find last dot
		size_t lastDot = filename.find_last_of('.');

		// No dot found, return the entire filename
		if (lastDot == std::string_view::npos)
		{
			return filename;
		}

		// Return substring up to (but not including) the last dot
		return filename.substr(0, lastDot);
	}
}
// The filename with extension of the current file
#define KOI_FILENAME ::koi::ExtractFilename(__FILE__)

// The filename without extension of the current file
#define KOI_FILE ::koi::ExtractFilenameNoExt(__FILE__)