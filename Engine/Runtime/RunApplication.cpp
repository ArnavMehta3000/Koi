#include "Runtime/AppBase.h"
#include "Runtime/Engine.h"
#include "Core/StringUtils.h"
#include <vector>
#include <string>

namespace koi::internal
{
	int RunApplication(int argc, wchar_t* wargv[])
	{
		std::vector<std::string> args(argc);
		std::vector<char*> argv(argc);
		for (int i = 0; i < argc; ++i)
		{
			args[i] = StringUtils::FromWide(wargv[i]);
			argv[i] = args[i].data();
		}
		return RunApplication(argc, argv.data());
	}

	int RunApplication(int argc, char* argv[])
	{
		assert(argc > 0 && "argc must be > 0");
		assert(argv != nullptr && "argv must not be nullptr");

		// Initialize the logger before the Engine so it outlives it during
		// stack unwinding — Engine::~Engine calls Logger::Shutdown(), which
		// would otherwise make IsInitialized() return false in catch blocks.
		Logger::Init();

		try
		{
			Engine engine;

			auto cmd = internal::InvokeCommandLineFactory();
			if (cmd->ParseArgs(argc, argv) != 0)
			{
				KOI_LOG_CRITICAL("Failed to parse command line arguments");
				return -1;
			}

			if (auto app = CreateApplication(*cmd))
			{
				engine.RunApp(*app);
				return 0;
			}
			else
			{
				KOI_LOG_CRITICAL("Failed to create application");
				return -1;
			}
		}
		catch (const std::exception& e)
		{
			KOI_LOG_CRITICAL("Unhandled exception: {}", e.what());
			Logger::Shutdown();
			return 1;
		}
		catch (...)
		{
			KOI_LOG_CRITICAL("Unhandled unknown exception");
			Logger::Shutdown();
			return 2;
		}
	}
}
