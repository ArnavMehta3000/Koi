#pragma once
#include <expected>
#include <format>
#include <string>
#include <utility>

namespace koi
{
	struct Error
	{
		std::string Message;

		template<typename... Args>
		[[nodiscard]] static std::unexpected<Error> Make(std::format_string<Args...> fmt, Args&&... args)
		{
			return std::unexpected(Error{ std::format(fmt, std::forward<Args>(args)...) });
		}
	};

	template<typename T>
	using Result = std::expected<T, Error>;
}
