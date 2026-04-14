#pragma once
#include "Core/Types.h"
#include "Platform/Win32.h"
#include "Kirin/Common/Platform.h"
#include <vector>
#include <optional>
#include <string>
#include <string_view>
#include <ranges>
#include <algorithm>
#include <cctype>
#include <charconv>

#if KRN_OS_WINDOWS
	#include <stringapiset.h>
#endif

namespace koi
{
	[[nodiscard]] inline std::wstring ToWide(std::string_view utf8)
	{
#if KRN_OS_WINDOWS
		if (utf8.empty())  return {};

		const i32 len = ::MultiByteToWideChar(
			CP_UTF8, 0, utf8.data(),
			static_cast<i32>(utf8.size()),
			nullptr, 0);

		if (len <= 0) return {};

		std::wstring out(static_cast<size_t>(len), L'\0');
		::MultiByteToWideChar(
			CP_UTF8, 0, utf8.data(),
			static_cast<i32>(utf8.size()), out.data(), len);

		return out;
#else
		return std::wstring(utf8.begin(), utf8.end());
#endif
	}

	[[nodiscard]] inline std::string FromWide(std::wstring_view ws)
	{
#if KRN_OS_WINDOWS
		if (ws.empty()) return {};

		const i32 len = ::WideCharToMultiByte(
			CP_UTF8, 0, ws.data(),
			static_cast<i32>(ws.size()),
			nullptr, 0, nullptr, nullptr);

		if (len <= 0) return {};

		std::string out(static_cast<size_t>(len), '\0');
		::WideCharToMultiByte(
			CP_UTF8, 0, ws.data(),
			static_cast<i32>(ws.size()),
			out.data(), len, nullptr, nullptr);

		return out;
#else
		return std::string(ws.begin(), ws.end());
#endif
	}

	[[nodiscard]] inline std::string ToLower(std::string_view sv)
	{
		std::string r(sv);
		std::ranges::transform(r, r.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return r;
	}

	[[nodiscard]] inline std::string ToUpper(std::string_view sv)
	{
		std::string r(sv);
		std::ranges::transform(r, r.begin(),
			[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
		return r;
	}

	inline void MakeLower(std::string& s)
	{
		std::ranges::transform(s, s.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	}

	inline void MakeUpper(std::string& s)
	{
		std::ranges::transform(s, s.begin(),
			[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
	}

	[[nodiscard]] inline std::string_view TrimLeftView(std::string_view sv) noexcept
	{
		auto it = std::ranges::find_if(sv,
			[](unsigned char c) { return !std::isspace(c); });
		return sv.substr(static_cast<size_t>(it - sv.begin()));
	}

	[[nodiscard]] inline std::string_view TrimRightView(std::string_view sv) noexcept
	{
		auto it = std::ranges::find_if(sv | std::views::reverse,
			[](unsigned char c) { return !std::isspace(c); });
		return sv.substr(0, static_cast<size_t>(sv.rend() - it));
	}

	[[nodiscard]] inline std::string_view TrimView(std::string_view sv) noexcept
	{
		return TrimRightView(TrimLeftView(sv));
	}

	[[nodiscard]] inline std::string TrimLeft(std::string_view sv) { return std::string(TrimLeftView(sv)); }
	[[nodiscard]] inline std::string TrimRight(std::string_view sv) { return std::string(TrimRightView(sv)); }
	[[nodiscard]] inline std::string Trim(std::string_view sv) { return std::string(TrimView(sv)); }

	inline void TrimInplace(std::string& s)
	{
		auto v = TrimView(s);
		if (v.data() != s.data() || v.size() != s.size())
		{
			s.assign(v);
		}
	}

	[[nodiscard]] inline bool EqualsCaseInsensitive(std::string_view a, std::string_view b) noexcept
	{
		if (a.size() != b.size())
			return false;

		for (size_t i = 0; i < a.size(); ++i)
		{
			if (std::tolower(static_cast<unsigned char>(a[i]))
				!= std::tolower(static_cast<unsigned char>(b[i])))
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline bool StartWithCaseInsensitive(std::string_view s, std::string_view prefix) noexcept
	{
		if (prefix.size() > s.size())
		{
			return false;
		}
		return EqualsCaseInsensitive(s.substr(0, prefix.size()), prefix);
	}

	[[nodiscard]] inline bool EndWithCaseInsensitive(std::string_view s, std::string_view suffix) noexcept
	{
		if (suffix.size() > s.size())
		{
			return false;
		}
		return EqualsCaseInsensitive(s.substr(s.size() - suffix.size()), suffix);
	}

	[[nodiscard]] inline bool contains_ci(std::string_view s, std::string_view needle) noexcept
	{
		if (needle.empty())
			return true;

		if (needle.size() > s.size())
			return false;

		const std::string lo_s = ToLower(s);
		const std::string lo_n = ToLower(needle);

		return lo_s.find(lo_n) != std::string::npos;
	}

	[[nodiscard]] inline std::vector<std::string> Split(std::string_view s, char delim, bool keep_empty = true)
	{
		size_t start = 0;
		std::vector<std::string> out;

		for (size_t i = 0; i <= s.size(); ++i)
		{
			if (i == s.size() || s[i] == delim)
			{
				if (keep_empty || i > start)
				{
					out.emplace_back(s.substr(start, i - start));
				}
				start = i + 1;
			}
		}
		return out;
	}

	[[nodiscard]] inline std::vector<std::string> Split(std::string_view s, std::string_view delim, bool keep_empty = true)
	{
		std::vector<std::string> out;
		if (delim.empty())
		{
			out.emplace_back(s); return out;
		}

		size_t start = 0, pos;
		while ((pos = s.find(delim, start)) != std::string_view::npos)
		{
			if (keep_empty || pos > start)
			{
				out.emplace_back(s.substr(start, pos - start));
			}
			start = pos + delim.size();
		}

		if (keep_empty || start < s.size())
		{
			out.emplace_back(s.substr(start));
		}

		return out;
	}

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

	inline void ReplaceAllInplace(std::string& s, std::string_view from, std::string_view to)
	{
		if (from.empty())
			return;

		size_t pos = 0;
		while ((pos = s.find(from, pos)) != std::string::npos)
		{
			s.replace(pos, from.size(), to);
			pos += to.size();
		}
	}

	[[nodiscard]] inline std::string Replace(std::string_view s, std::string_view from, std::string_view to)
	{
		std::string r(s);
		ReplaceAllInplace(r, from, to);
		return r;
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
}
