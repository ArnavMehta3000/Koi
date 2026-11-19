#include "Koi/Engine/Platform/Paths.h"
#include "Koi/Engine/Core/Win32.h"

namespace koi::utils
{
	fs::path ExecutableDir()
	{
		wchar_t buffer[MAX_PATH];
		const DWORD len = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
		return fs::path(buffer, buffer + len).parent_path();
	}
}
