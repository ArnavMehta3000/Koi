#include "Koi/Engine/Text/StringUtils.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Platform/Win32.h"

namespace koi
{
	std::wstring StringUtils::ToWide(std::string_view utf8)
	{
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
	}

	std::string StringUtils::FromWide(std::wstring_view ws)
	{
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
	}
}
