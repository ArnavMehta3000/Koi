// Contains macros to create the platform independent main  function

#pragma once
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/Win32.h"
#include <filesystem>
#include <memory>

namespace koi
{
	class AppBase;
	class IAppHost;
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
	struct LaunchDesc;

	// Pointer to wrap host
	using HostAdapter = std::unique_ptr<IAppHost>(*)(std::unique_ptr<AppBase>, const LaunchDesc&);

	// Editor defines them and runtime leave them empty to find cooked content in the sam dir
	struct LaunchDesc
	{
		std::filesystem::path EngineDir;        // <repo>/Koi/Engine (editor only)
		std::filesystem::path ProjectDir;       // game source root (editor only)
		std::filesystem::path ProjectFile;      // default project TOML; -p overrides
		std::filesystem::path EngineBuildDir;   // xmake output (compiled shaders) mounted beside EngineDir (editor only)
		std::filesystem::path ProjectBuildDir;  // same, beside ProjectDir
		HostAdapter Adapter = nullptr;
	};

	int RunApplication(int argc, char* argv[], const LaunchDesc& desc);
	int RunApplication(int argc, wchar_t* wargv[], const LaunchDesc& desc);
}

#if KOI_DEBUG
	#define KOI_IMPLEMENT_MAIN_WITH(launch)                                 \
		KOI_MAIN()                                                          \
		{                                                                   \
			return ::koi::internal::RunApplication(argc, argv, (launch)()); \
		}
#else
	#define KOI_IMPLEMENT_MAIN_WITH(launch)                                      \
		KOI_MAIN()                                                               \
		{                                                                        \
			return ::koi::internal::RunApplication(__argc, __wargv, (launch)()); \
		}
#endif

#define KOI_IMPLEMENT_MAIN() KOI_IMPLEMENT_MAIN_WITH([] { return ::koi::internal::LaunchDesc{}; })
