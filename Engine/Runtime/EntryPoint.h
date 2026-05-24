// Contains macros to create the platform independent main  function

#pragma once
#include "Core/Core.h"
#include "Platform/Win32.h"


// Default main
#define KOI_DEF_MAIN() int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]);

#define KOI_DEF_WINMAIN()                              \
int WINAPI wWinMain(                                   \
	_In_     [[maybe_unused]] HINSTANCE hInstance,     \
	_In_opt_ [[maybe_unused]] HINSTANCE hPrevInstance, \
	_In_     [[maybe_unused]] LPWSTR    lpCmdLine,     \
	_In_     [[maybe_unused]] int       nCmdShow)


#if KOI_DEBUG
	// Use default main in debug mode (enables native console)
	#define KOI_MAIN() KOI_DEF_MAIN()
#else
	// Use WinMain in non-debug modes (manually create console via logger)
	#define KOI_MAIN() KOI_DEF_WINMAIN()
#endif

namespace koi::internal
{
	int RunApplication(int argc, char* argv[]);
	int RunApplication(int argc, wchar_t* wargv[]);
}

#if KOI_DEBUG
	#define KOI_IMPLEMENT_MAIN()                                \
		KOI_MAIN()                                              \
		{                                                       \
			return ::koi::internal::RunApplication(argc, argv); \
		}
#else
	#define KOI_IMPLEMENT_MAIN()                                     \
		KOI_MAIN()                                                   \
		{                                                            \
			return ::koi::internal::RunApplication(__argc, __wargv); \
		}
#endif
