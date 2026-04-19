// Contains macros to create the platform independent main  function

#pragma once
#include "Core/Core.h"

#if KRN_OS_WINDOWS
	#include "Platform/Win32/Win32.h"
#endif

// Default main
#define KOI_DEF_MAIN() int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])

#if KRN_OS_WINDOWS
	#define KOI_DEF_WINMAIN()                              \
	int WINAPI wWinMain(                                   \
		_In_     [[maybe_unused]] HINSTANCE hInstance,     \
		_In_opt_ [[maybe_unused]] HINSTANCE hPrevInstance, \
		_In_     [[maybe_unused]] LPWSTR    lpCmdLine,     \
		_In_     [[maybe_unused]] int       nCmdShow)
#else
	// No WinMain on non-windows platforms
	#define KOI_DEF_WINMAIN() KOI_DEF_MAIN()
#endif


#if KOI_DEBUG
	// Use default main in debug mode (enables native console)
	#define KOI_MAIN() KOI_DEF_MAIN()
#else
	// Use WinMain in non-debug modes (manually create console via logger)
	#define KOI_MAIN() KOI_DEF_WINMAIN()
#endif

namespace koi::internal
{
	int RunApplication(int argc, char** argv);
}

#if KOI_DEBUG || !KRN_OS_WINDOWS
	#define KOI_IMPLEMENT_MAIN()                                \
		KOI_DEF_MAIN()                                          \
		{                                                       \
			return ::koi::internal::RunApplication(argc, argv); \
		}
#else
	#define KOI_IMPLEMENT_MAIN()                                    \
		KOI_DEF_WINMAIN()                                           \
		{                                                           \
			return ::koi::internal::RunApplication(__argc, __argv); \
		}
#endif
