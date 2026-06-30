module;
#include "Core/Config.h"
#include <filesystem>
#include <vector>
#include <string>
#include <cassert>
module Koi.Runtime.AppBase;

import Koi.Core.Logger;
import Koi.Core.StringUtils;
import Koi.Core.VirtualFileSystem;
import Koi.Runtime.Engine;
import Koi.Runtime.CommandLine;
#include "Core/Profiling.h"
#include "Core/Logger.h"

namespace koi::internal
{
	static std::unique_ptr<AppBase> CreateApplicationInstance(CommandLine& cmd)
	{
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

		Logger::Init();

		try
		{
			auto cmd = internal::InvokeCommandLineFactory();
			if (cmd->ParseArgs(argc, argv) != 0)
			{
				KOI_LOG_CRITICAL("Failed to parse command line arguments");
				return -1;
			}

			{
				const fs::path exeDir = fs::current_path();
#if KOI_WITH_EDITOR
				const fs::path projectRoot = cmd->GetProjectFile().empty()
					? exeDir
					: fs::absolute(cmd->GetProjectFile()).parent_path();
				const fs::path engineRoot = exeDir / "Engine";
#else
				fs::path contentRoot = exeDir;
				if (!fs::exists(contentRoot / "Project" / "Project.toml") &&
				     fs::exists(exeDir / "Cooked" / "Project" / "Project.toml"))
				{
					contentRoot = exeDir / "Cooked";
				}

				if (cmd->GetProjectFile().empty())
				{
					const fs::path defaultProject = contentRoot / "Project" / "Project.toml";
					if (fs::exists(defaultProject))
					{
						KOI_LOG_INFO("No --project specified; defaulting to {}", defaultProject.string());
						cmd->OverrideProjectFile(defaultProject);
					}
				}

				const fs::path projectRoot = contentRoot / "Project";
				const fs::path engineRoot  = contentRoot / "Engine";
#endif
#if KOI_DEBUG
				KOI_LOG_DEBUG("Setting up runtime VFS mounts");
				KOI_LOG_DEBUG("\tWorking directory : {}", exeDir.string());
				KOI_LOG_DEBUG("\tProject file      : {}", cmd->GetProjectFile().string());
				KOI_LOG_DEBUG("\tEngine root       : {}", engineRoot.string());
				KOI_LOG_DEBUG("\tProject root      : {}", projectRoot.string());
#endif
				auto& vfs = VirtualFileSystem::Get();
				vfs.Mount("engine", std::make_unique<FilesystemMount>(engineRoot));
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
