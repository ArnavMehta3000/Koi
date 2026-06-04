#include "Runtime/AppBase.h"
#include "Runtime/Engine.h"
#include "Core/StringUtils.h"
#include "Core/Profiling.h"
#include "Core/VirtualFileSystem.h"
#include <filesystem>
#include <vector>
#include <string>

namespace koi::internal
{
	static std::unique_ptr<AppBase> CreateApplicationInstance(CommandLine& cmd)
	{
		// Custom scoped function for profiling reasons
		ZoneScoped;
		return CreateApplication(cmd);
	}

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
			auto cmd = internal::InvokeCommandLineFactory();
			if (cmd->ParseArgs(argc, argv) != 0)
			{
				KOI_LOG_CRITICAL("Failed to parse command line arguments");
				return -1;
			}

			// In runtime/standalone mode, if '-p' is not specified. Look for a 'Project.toml' next to the cwd
			// And override the project file arg in the commandline
			if (cmd->GetProjectFile().empty())
			{
				const fs::path defaultProject = fs::current_path() / "Project.toml";
				if (fs::exists(defaultProject))
				{
					KOI_LOG_INFO("No --project specified; defaulting to {}", defaultProject.string());
					cmd->OverrideProjectFile(defaultProject);
				}
			}

			// Set up VFS before engine init
			//   Runtime: assets are baked next to the exe — project mount is
			//           the exe dir, engine mount is exe/Engine.
			//   Editor:  assets live in source — project mount follows the
			//           --project TOML's parent dir, engine mount follows
			//           the engine assets shipped next to the editor exe.
			{
				const fs::path exeDir = fs::current_path();
#if KOI_WITH_EDITOR
				const fs::path projectRoot = cmd->GetProjectFile().empty()
					? exeDir  // No project file specified, default to current dir
					: fs::absolute(cmd->GetProjectFile()).parent_path();
#else
				const fs::path projectRoot = exeDir;
#endif
				auto& vfs = VirtualFileSystem::Get();
				vfs.Mount("engine", std::make_unique<FilesystemMount>(exeDir / "Engine"));
				vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoot));
			}

			Engine engine;

			if (auto app = CreateApplicationInstance(*cmd))
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
