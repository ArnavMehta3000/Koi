#include "Koi/Engine/Runtime/EntryPoint.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Platform/Paths.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <toml++/toml.hpp>
#include <cassert>
#include <filesystem>
#include <map>
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
		const toml::table* input = project["input"].as_table();

		std::map<std::string, std::string> scenes;
		if (const toml::table* list = project["scenes"]["list"].as_table())
		{
			for (const auto& [name, node] : *list)
			{
				scenes.emplace(std::string(name.str()), node.value_or(""));
			}
		}

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
			.Input          = input ? LoadInputConfig(*input) : InputConfig{},
			.AppName        = project["project"]["name"].value_or("Koi Application"),
			.Scenes         = std::move(scenes),
			.StartupScene   = project["scenes"]["startup"].value_or(""),
		};
	}

	// Editor: source trees are baked into the launcher
	static void MountSourceTree(const LaunchDesc& desc)
	{
		auto& vfs = VirtualFileSystem::Get();
		vfs.Mount("engine",  std::make_unique<FilesystemMount>(desc.EngineDir));
		vfs.Mount("project", std::make_unique<FilesystemMount>(desc.ProjectDir));

		KOI_LOG_DEBUG("Source VFS mounts:");
		KOI_LOG_DEBUG("\tengine  <- {}", desc.EngineDir.string());
		KOI_LOG_DEBUG("\tproject <- {}", desc.ProjectDir.string());
	}

	// Runtime: cooked content is laid out as <root>/Engine + <root>/Project, where root is
	// the exe dir or, when launched from the build tree, the CWD
	static fs::path FindCookedRoot(const fs::path& exeDir)
	{
		const fs::path cwd = fs::current_path();
		for (const fs::path& root : { exeDir, exeDir / "Cooked", cwd, cwd / "Cooked" })
		{
			if (fs::exists(root / "Project" / "Project.koi.toml"))
				return root;
		}
		return exeDir;
	}

	static void MountCooked(CommandLine& cmd, const fs::path& exeDir)
	{
		const fs::path contentRoot = FindCookedRoot(exeDir);
		const fs::path projectRoot = contentRoot / "Project";
		const fs::path engineRoot  = contentRoot / "Engine";

		if (cmd.GetProjectFile().empty())
			cmd.OverrideProjectFile(projectRoot / "Project.koi.toml");

		auto& vfs = VirtualFileSystem::Get();
		vfs.Mount("engine",  std::make_unique<FilesystemMount>(engineRoot));
		vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoot));

		KOI_LOG_DEBUG("Cooked VFS mounts:");
		KOI_LOG_DEBUG("\tProject file : {}", cmd.GetProjectFile().string());
		KOI_LOG_DEBUG("\tengine  <- {}", engineRoot.string());
		KOI_LOG_DEBUG("\tproject <- {}", projectRoot.string());
	}

	// Owns the logger for the whole of RunApplication so it outlives the Engine and
	// stays alive inside the catch handlers during stack unwinding
	struct LoggerScope
	{
		LoggerScope()  { Logger::Init({ .FilePath = "" }); }  // no file until .koi/ is known
		~LoggerScope() { Logger::Shutdown(); }
	};

	// Relative log paths land in the project's .koi/ (editor) or beside the exe (runtime)
	static void ResolveLogPath(LoggerConfig& log, const fs::path& savedDir)
	{
		if (log.FilePath.empty() || fs::path(log.FilePath).is_absolute()) return;
		log.FilePath = (savedDir / log.FilePath).string();
	}

	int RunApplication(int argc, char* argv[], const LaunchDesc& desc)
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

			const fs::path exeDir   = Paths::ExecutableDir();
			const bool     isEditor = !desc.ProjectDir.empty();
			const fs::path savedDir = (isEditor ? desc.ProjectDir : exeDir) / ".koi";

			if (isEditor)
			{
				if (cmd->GetProjectFile().empty())
				{
					cmd->OverrideProjectFile(desc.ProjectFile);
				}

				MountSourceTree(desc);
			}
			else
			{
				MountCooked(*cmd, exeDir);
			}

			Engine engine;

			KOI_LOG_DEBUG("Creating application instance");
			std::unique_ptr<AppBase> app = CreateApplicationInstance(*cmd);
			if (!app)
			{
				KOI_LOG_CRITICAL("Failed to create application");
				return -1;
			}

			AppCreateDesc appDesc = LoadProjectDesc(cmd->GetProjectFile());
			ResolveLogPath(appDesc.LogConfig, savedDir);
			app->SetDesc(std::move(appDesc));

			// Only now is the project's [logging] table known; still single-threaded here
			Logger::Configure(app->GetDesc().LogConfig);

			if (desc.Adapter)
			{
				// Editor build: wrap the game in an EditorApp supplied by the launcher
				KOI_LOG_DEBUG("Host adapter present; wrapping application for the editor");
				LaunchDesc resolved = desc;
				resolved.ProjectFile = cmd->GetProjectFile();

				std::unique_ptr<IAppHost> host = desc.Adapter(std::move(app), resolved);
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

	int RunApplication(int argc, wchar_t* wargv[], const LaunchDesc& desc)
	{
		std::vector<std::string> args(argc);
		std::vector<char*> argv(argc);
		for (int i = 0; i < argc; ++i)
		{
			args[i] = StringUtils::FromWide(wargv[i]);
			argv[i] = args[i].data();
		}
		return RunApplication(argc, argv.data(), desc);
	}
}
