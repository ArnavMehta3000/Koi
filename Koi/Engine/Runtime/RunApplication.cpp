#include "Koi/Engine/Runtime/EntryPoint.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <toml++/toml.hpp>
#include <cassert>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace koi::internal
{
	static std::unique_ptr<AppBase> CreateApplicationInstance(CommandLine& cmd)
	{
		// Custom scoped function for profiling reasons
		ZoneScoped;
		return CreateApplication(cmd);
	}

	static AppCreateDesc LoadProjectDesc(const fs::path& projectFile)
	{
		ZoneScoped;
		const toml::table project = toml::parse_file(projectFile.string());

		const LoggerConfig defaults{};

		// Engine-defined data only
		return AppCreateDesc
		{
			.WindowDesc =
			{
				.Title  = project["window"]["title"].value_or("Koi"),
				.Width  = project["window"]["width"].value_or(1280u),
				.Height = project["window"]["height"].value_or(720u),
				.VSync  = project["window"]["vsync"].value_or(true),
			},
			.LogConfig =
			{
				.FilePath        = project["logging"]["file"].value_or(defaults.FilePath),
				.RuntimeLevel    = LogLevelFromString(project["logging"]["level"].value_or(ToString(defaults.RuntimeLevel))),
				.HistoryCapacity = project["logging"]["history"].value_or(defaults.HistoryCapacity),
				.SpawnConsole    = project["logging"]["console"].value_or(defaults.SpawnConsole),
				.UseDebugSink    = project["logging"]["debug_sink"].value_or(defaults.UseDebugSink),
			},
			.AppName        = project["project"]["name"].value_or("Koi Application"),
			.StartupScene   = project["assets"]["startup_scene"].value_or(""),
		};
	}

	// Source-tree mode: --engine-source / --project-source layer on top of the exe-relative fallbacks
	static void MountSourceTree(const CommandLine& cmd, const fs::path& exeDir)
	{
		std::vector<fs::path> engineRoots;
		if (!cmd.GetEngineSource().empty())
		{
			engineRoots.push_back(fs::absolute(cmd.GetEngineSource()));
		}
		engineRoots.push_back(exeDir / "Engine");

		std::vector<fs::path> projectRoots;
		projectRoots.push_back(fs::absolute(cmd.GetProjectSource()));
		projectRoots.push_back(exeDir);

		auto& vfs = VirtualFileSystem::Get();
		vfs.Mount("engine",  std::make_unique<FilesystemMount>(engineRoots));
		vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoots));

		KOI_LOG_DEBUG("Source VFS mounts (working dir {}):", exeDir.string());
		for (const fs::path& p : engineRoots) { KOI_LOG_DEBUG("\tengine  <- {}", p.string()); }
		for (const fs::path& p : projectRoots) { KOI_LOG_DEBUG("\tproject <- {}", p.string()); }
	}

	// Cooked mode: content lives next to the exe (or under Cooked/), laid out as <root>/Engine + <root>/Project
	static void MountCooked(CommandLine& cmd, const fs::path& exeDir)
	{
		fs::path contentRoot = exeDir;
		if (!fs::exists(contentRoot / "Project" / "Project.koi.toml") &&
		     fs::exists(exeDir / "Cooked" / "Project" / "Project.koi.toml"))
		{
			contentRoot = exeDir / "Cooked";
		}

		if (cmd.GetProjectFile().empty())
		{
			const fs::path defaultProject = contentRoot / "Project" / "Project.koi.toml";
			if (fs::exists(defaultProject))
			{
				KOI_LOG_INFO("No --project specified; defaulting to {}", defaultProject.string());
				cmd.OverrideProjectFile(defaultProject);
			}
		}

		const fs::path projectRoot = contentRoot / "Project";
		const fs::path engineRoot  = contentRoot / "Engine";

		auto& vfs = VirtualFileSystem::Get();
		vfs.Mount("engine",  std::make_unique<FilesystemMount>(engineRoot));
		vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoot));

		KOI_LOG_DEBUG("Cooked VFS mounts:");
		KOI_LOG_DEBUG("\tWorking directory : {}", exeDir.string());
		KOI_LOG_DEBUG("\tProject file      : {}", cmd.GetProjectFile().string());
		KOI_LOG_DEBUG("\tEngine root       : {}", engineRoot.string());
		KOI_LOG_DEBUG("\tProject root      : {}", projectRoot.string());

	}

	// Owns the logger for the whole of RunApplication so it outlives the Engine and
	// stays alive inside the catch handlers during stack unwinding
	struct LoggerScope
	{
		LoggerScope()  { Logger::Init(); }
		~LoggerScope() { Logger::Shutdown(); }
	};

	int RunApplication(int argc, char* argv[], HostAdapter adapter)
	{
		assert(argc > 0 && "argc must be > 0");
		assert(argv != nullptr && "argv must not be nullptr");

		LoggerScope logger;

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
				if (!cmd->GetProjectSource().empty())
				{
					MountSourceTree(*cmd, exeDir);  // With editor (read from disk)
				}
				else
				{
					MountCooked(*cmd, exeDir);  // Runtime/Standalone (read cooked)
				}
			}

			Engine engine;

			KOI_LOG_DEBUG("Creating application instance");
			std::unique_ptr<AppBase> app = CreateApplicationInstance(*cmd);
			if (!app)
			{
				KOI_LOG_CRITICAL("Failed to create application");
				return -1;
			}

			app->SetDesc(LoadProjectDesc(cmd->GetProjectFile()));

			// Only now is the project's [logging] table known; still single-threaded here
			Logger::Configure(app->GetDesc().LogConfig);

			if (adapter)
			{
				// Editor build: wrap the game in an EditorApp supplied by the launcher
				KOI_LOG_DEBUG("Host adapter present; wrapping application for the editor");
				std::unique_ptr<IAppHost> host = adapter(std::move(app), *cmd);
				if (!host)
				{
					KOI_LOG_CRITICAL("Host adapter returned null");
					return -1;
				}
				engine.RunApp(*host);
			}
			else
			{
				// Standalone build: run the game directly
				engine.RunApp(*app);
			}
			return 0;
		}
		catch (const std::exception& e)
		{
			KOI_LOG_CRITICAL("Unhandled exception: {}", e.what());
			return 1;
		}
		catch (...)
		{
			KOI_LOG_CRITICAL("Unhandled unknown exception");
			return 2;
		}
	}

	int RunApplication(int argc, char* argv[])
	{
		return RunApplication(argc, argv, nullptr);
	}

	int RunApplication(int argc, wchar_t* wargv[], HostAdapter adapter)
	{
		std::vector<std::string> args(argc);
		std::vector<char*> argv(argc);
		for (int i = 0; i < argc; ++i)
		{
			args[i] = StringUtils::FromWide(wargv[i]);
			argv[i] = args[i].data();
		}
		return RunApplication(argc, argv.data(), adapter);
	}

	int RunApplication(int argc, wchar_t* wargv[])
	{
		return RunApplication(argc, wargv, nullptr);
	}
}
