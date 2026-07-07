// Contains utility types and functions used throughout the engine
#pragma once
#include <cstdint>
#include <string_view>

namespace koi
{
	/**
	* @brief Signed 8-bit integer
	*/
	using i8  = std::int8_t;

	/**
	* @brief Signed 16-bit integer
	*/
	using i16 = std::int16_t;

	/**
	* @brief Signed 32-bit integer
	*/
	using i32 = std::int32_t;

	/**
	* @brief Signed 64-bit integer
	*/
	using i64 = std::int64_t;


	/**
	* @brief Unsigned 8-bit integer
	*/
	using u8  = std::uint8_t;

	/**
	* @brief Unsigned 16-bit integer
	*/
	using u16 = std::uint16_t;

	/**
	* @brief Unsigned 32-bit integer
	*/
	using u32 = std::uint32_t;

	/**
	* @brief Unsigned 64-bit integer
	*/
	using u64 = std::uint64_t;

	/**
	* @brief 32-bit floating point
	*/
	using f32  = float;

	/**
	* @brief 64-bit floating point
	*/
	using f64 = double;

	/**
	* @brief 8-bit byte
	*/
	using byte = u8;

	static_assert(sizeof(u8) == 1);
	static_assert(sizeof(u16) == 2);
	static_assert(sizeof(u32) == 4);
	static_assert(sizeof(u64) == 8);
	static_assert(sizeof(f32) == 4);
	static_assert(sizeof(f64) == 8);
	static_assert(sizeof(byte) == 1);

	struct NonCopyable
	{
		NonCopyable(const NonCopyable&) = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	protected:
		NonCopyable() = default;
		~NonCopyable() = default;
	};

	struct NonMovable
	{
		NonMovable(NonMovable&&) = delete;
		NonMovable& operator=(NonMovable&&) = delete;
	protected:
		NonMovable() = default;
		~NonMovable() = default;
	};



	/// No better place to put this
	consteval std::string_view ExtractFilename(std::string_view path) noexcept
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

	consteval std::string_view ExtractFilenameNoExt(std::string_view path) noexcept
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
