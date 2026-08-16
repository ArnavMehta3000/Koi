// Contains macros to create the platform independent main  function

#pragma once
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Platform/Win32.h"
#include <memory>

namespace koi
{
	class AppBase;
	class IAppHost;
	class CommandLine;
}


// Default main
#define KOI_DEF_MAIN() int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])

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
	// Pointer to wrap host (editor will wrap user AppBase)
	using HostAdapter = std::unique_ptr<IAppHost>(*)(std::unique_ptr<AppBase>, CommandLine&);

	int RunApplication(int argc, char* argv[]);
	int RunApplication(int argc, wchar_t* wargv[]);
	int RunApplication(int argc, char* argv[], HostAdapter adapter);
	int RunApplication(int argc, wchar_t* wargv[], HostAdapter adapter);
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

// For editor use
#if KOI_DEBUG
	#define KOI_IMPLEMENT_MAIN_WITH_HOST(adapter)                        \
		KOI_MAIN()                                                       \
		{                                                                \
			return ::koi::internal::RunApplication(argc, argv, adapter); \
		}
#else
	#define KOI_IMPLEMENT_MAIN_WITH_HOST(adapter)                             \
		KOI_MAIN()                                                            \
		{                                                                     \
			return ::koi::internal::RunApplication(__argc, __wargv, adapter); \
		}
#endif
