#pragma once
#include <string>
#include <string_view>

namespace koi::utils
{
	// UTF-8 <-> UTF-16, for the Win32 / D3D12 / DXC boundary
	[[nodiscard]] std::wstring ToWide(std::string_view utf8);
	[[nodiscard]] std::string FromWide(std::wstring_view ws);
}
