module;
#include <cstdint>
#include <string_view>
export module Koi.Core.Types;

export namespace koi
{
	using i8  = std::int8_t;
	using i16 = std::int16_t;
	using i32 = std::int32_t;
	using i64 = std::int64_t;
	using u8  = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;
	using f32 = float;
	using f64 = double;
	using byte = u8;

	static_assert(sizeof(u8)  == 1);
	static_assert(sizeof(u16) == 2);
	static_assert(sizeof(u32) == 4);
	static_assert(sizeof(u64) == 8);
	static_assert(sizeof(f32) == 4);
	static_assert(sizeof(f64) == 8);
	static_assert(sizeof(byte) == 1);

	struct NonCopyable
	{
		NonCopyable(const NonCopyable&)            = delete;
		NonCopyable& operator=(const NonCopyable&) = delete;
	protected:
		NonCopyable()  = default;
		~NonCopyable() = default;
	};

	struct NonMovable
	{
		NonMovable(NonMovable&&)            = delete;
		NonMovable& operator=(NonMovable&&) = delete;
	protected:
		NonMovable()  = default;
		~NonMovable() = default;
	};

	consteval std::string_view ExtractFilename(std::string_view path) noexcept
	{
		size_t lastSlash = path.find_last_of("/\\");
		if (lastSlash == std::string_view::npos)
			return path;
		return path.substr(lastSlash + 1);
	}

	consteval std::string_view ExtractFilenameNoExt(std::string_view path) noexcept
	{
		std::string_view filename = ExtractFilename(path);
		size_t lastDot = filename.find_last_of('.');
		if (lastDot == std::string_view::npos)
			return filename;
		return filename.substr(0, lastDot);
	}
}
