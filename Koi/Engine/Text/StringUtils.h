#pragma once
#include <string>
#include <string_view>

namespace koi
{
	// UTF-8 <-> UTF-16, for the Win32 / D3D12 / DXC boundary
	class StringUtils
	{
	public:
		[[nodiscard]] static std::wstring ToWide(std::string_view utf8);
		[[nodiscard]] static std::string FromWide(std::wstring_view ws);
	};
}
