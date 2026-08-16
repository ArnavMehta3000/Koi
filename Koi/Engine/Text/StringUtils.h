// Contains utility functions for string manipulation in static StringUtils class
#pragma once
#include <charconv>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace koi
{
	class StringUtils
	{
	public:
		[[nodiscard]] static std::wstring ToWide(std::string_view utf8);
		[[nodiscard]] static std::string FromWide(std::wstring_view ws);
		[[nodiscard]] static std::string ToLower(std::string_view sv);
		[[nodiscard]] static std::string ToUpper(std::string_view sv);
		static void MakeLower(std::string& s);
		static void MakeUpper(std::string& s);
		[[nodiscard]] static std::string_view TrimLeftView(std::string_view sv) noexcept;
		[[nodiscard]] static std::string_view TrimRightView(std::string_view sv) noexcept;
		[[nodiscard]] static std::string_view TrimView(std::string_view sv) noexcept;
		[[nodiscard]] static std::string TrimLeft(std::string_view sv) { return std::string(TrimLeftView(sv)); }
		[[nodiscard]] static std::string TrimRight(std::string_view sv) { return std::string(TrimRightView(sv)); }
		[[nodiscard]] static std::string Trim(std::string_view sv) { return std::string(TrimView(sv)); }
		static void TrimInplace(std::string& s);
		[[nodiscard]] static bool EqualsCaseInsensitive(std::string_view a, std::string_view b) noexcept;
		[[nodiscard]] static bool StartWithCaseInsensitive(std::string_view s, std::string_view prefix) noexcept;
		[[nodiscard]] static bool EndWithCaseInsensitive(std::string_view s, std::string_view suffix) noexcept;
		[[nodiscard]] static bool ContainsCaseInsensitive(std::string_view s, std::string_view needle) noexcept;
		[[nodiscard]] static std::vector<std::string> Split(std::string_view s, char delim, bool keep_empty = true);
		[[nodiscard]] static std::vector<std::string> Split(std::string_view s, std::string_view delim, bool keep_empty = true);
		static void ReplaceAllInplace(std::string& s, std::string_view from, std::string_view to);
		[[nodiscard]] static std::string Replace(std::string_view s, std::string_view from, std::string_view to);

		template <std::ranges::input_range R>
		[[nodiscard]] std::string Join(const R& parts, std::string_view sep)
		{
			std::string out;
			bool first = true;
			for (const auto& p : parts)
			{
				if (!first)
					out.append(sep);

				out.append(std::string_view{ p });
				first = false;
			}
			return out;
		}

		template <typename T>
		[[nodiscard]] std::optional<T> ToNumber(std::string_view sv) noexcept
		{
			T value{};
			const auto* first = sv.data();
			const auto* last = sv.data() + sv.size();

			auto [ptr, ec] = std::from_chars(first, last, value);
			if (ec == std::errc{} && ptr == last)
			{
				return value;
			}
			return std::nullopt;
		}
	};
}
