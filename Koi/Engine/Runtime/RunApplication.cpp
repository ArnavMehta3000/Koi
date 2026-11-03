#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/CrashHandler.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Paths.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/AppHost.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/EntryPoint.h"
#include "Koi/Engine/Text/StringUtils.h"
#include <toml++/toml.hpp>
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
		return AppBase::Create(cmd);
	}

	static WindowMode ParseWindowMode(std::string_view mode)
	{
		if (mode == "windowed")   return WindowMode::Windowed;
		if (mode == "borderless") return WindowMode::Borderless;
		if (mode == "fullscreen") return WindowMode::Fullscreen;

		KOI_LOG(Engine, Warn, "[window] mode = '{}' is not windowed, borderless or fullscreen; using windowed", mode);
		return WindowMode::Windowed;
	}

	static AppCreateDesc LoadProjectDesc(const fs::path& projectFile)
	{
		ZoneScoped;
		const toml::table project = toml::parse_file(projectFile.string());

		// Try load input data
		const toml::table* input = project["input"].as_table();

		// Load scene list
		std::map<std::string, std::string> scenes;
		if (const toml::table* list = project["scenes"]["list"].as_table())
		{
			for (const auto& [name, node] : *list)
			{
				scenes.emplace(std::string(name.str()), node.value_or(""));
			}
		}

		// Load logging categories
		std::map<std::string, LogLevel, std::less<>> categoryLevels;
		if (const toml::table* categories = project["logging"]["categories"].as_table())
		{
			for (const auto& [name, node] : *categories)
			{
				const std::string_view level = node.value_or(std::string_view());
				if (utils::ToString(utils::LogLevelFromString(level)) != level)
				{
					KOI_LOG(Engine, Warn, "[logging.categories] {} = '{}' is not a log level", name.str(), level);
					continue;
				}
				categoryLevels.emplace(std::string(name.str()), utils::LogLevelFromString(level));
			}
		}

		const LoggerConfig defaults{};
		return AppCreateDesc
		{
			.WindowDesc =
			{
				.Title  = project["window"]["title"].value_or("Koi"),
				.Mode   = ParseWindowMode(project["window"]["mode"].value_or("windowed")),
				.Width  = project["window"]["width"].value_or(1280u),
				.Height = project["window"]["height"].value_or(720u),
				.VSync  = project["window"]["vsync"].value_or(true),
			},
			.LogConfig =
			{
				.FilePath        = project["logging"]["file"].value_or(defaults.FilePath),
				.RuntimeLevel    = utils::LogLevelFromString(project["logging"]["level"].value_or(utils::ToString(defaults.RuntimeLevel))),
				.HistoryCapacity = project["logging"]["history"].value_or(defaults.HistoryCapacity),
				.SpawnConsole    = project["logging"]["console"].value_or(defaults.SpawnConsole),
				.UseDebugSink    = project["logging"]["debug_sink"].value_or(defaults.UseDebugSink),
				.CategoryLevels  = std::move(categoryLevels),
			},
			.Input          = input ? InputConfig::Load(*input) : InputConfig{},
			.AppName        = project["project"]["name"].value_or("Koi Application"),
			.Scenes         = std::move(scenes),
			.StartupScene   = project["scenes"]["startup"].value_or(""),
		};
	}

	// --log entries win over the TOML; all=<level> resets every category before the named ones apply
	static void ApplyLogOverrides(LoggerConfig& log, const std::vector<std::string>& entries)
	{
		std::vector<std::pair<std::string_view, LogLevel>> named;
		for (std::string_view entry : entries)
		{
			const size_t eq = entry.find('=');
			const std::string_view name  = entry.substr(0, eq);
			const std::string_view level = eq == std::string_view::npos ? std::string_view() : entry.substr(eq + 1);
			if (name.empty() || utils::ToString(utils::LogLevelFromString(level)) != level)
			{
				KOI_LOG(Engine, Warn, "--log {}: expected Category=Level", entry);
				continue;
			}

			if (name == "all")
			{
				log.RuntimeLevel = utils::LogLevelFromString(level);
				log.CategoryLevels.clear();
			}
			else
			{
				named.emplace_back(name, utils::LogLevelFromString(level));
			}
		}

		for (const auto& [name, level] : named)
		{
			log.CategoryLevels.insert_or_assign(std::string(name), level);
		}
	}

	// Editor: source trees are baked into the launcher, with the build output (compiled shaders) behind them
	static void MountSourceTree(const LaunchDesc& desc)
	{
		std::vector<fs::path> engineRoots  = { desc.EngineDir,  desc.EngineBuildDir  };
		std::vector<fs::path> projectRoots = { desc.ProjectDir, desc.ProjectBuildDir };

		auto& vfs = VirtualFileSystem::Get();
		vfs.Mount("engine",  std::make_unique<FilesystemMount>(engineRoots));
		vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoots));

		KOI_LOG(Engine, Debug, "Source VFS mounts:");
		KOI_LOG(Engine, Debug, "\tengine  <- {} (+ {})", desc.EngineDir.string(),  desc.EngineBuildDir.string());
		KOI_LOG(Engine, Debug, "\tproject <- {} (+ {})", desc.ProjectDir.string(), desc.ProjectBuildDir.string());
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

	static void MountCooked(const fs::path& contentRoot)
	{
		const fs::path projectRoot = contentRoot / "Project";
		const fs::path engineRoot  = contentRoot / "Engine";

		auto& vfs = VirtualFileSystem::Get();
		vfs.Mount("engine",  std::make_unique<FilesystemMount>(engineRoot));
		vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoot));

		KOI_LOG(Engine, Debug, "Cooked VFS mounts:");
		KOI_LOG(Engine, Debug, "\tengine  <- {}", engineRoot.string());
		KOI_LOG(Engine, Debug, "\tproject <- {}", projectRoot.string());
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
		KOI_ASSERT(argc > 0, "argc must be > 0");
		KOI_ASSERT(argv != nullptr, "argv must not be nullptr");

		LoggerScope logger;
		CrashHandler::Install();

		try
		{
			auto cmd = internal::InvokeCommandLineFactory();
			if (cmd->ParseArgs(argc, argv) != 0)
			{
				KOI_LOG(Engine, Critical, "Failed to parse command line arguments");
				return -1;
			}

			const fs::path exeDir   = utils::ExecutableDir();
			const bool     isEditor = !desc.ProjectDir.empty();
			const fs::path savedDir = (isEditor ? desc.ProjectDir : exeDir) / ".koi";
			CrashHandler::SetDumpDir(savedDir / "Crashes");

			const fs::path cookedRoot = isEditor ? fs::path() : FindCookedRoot(exeDir);
			if (cmd->GetProjectFile().empty())
			{
				cmd->OverrideProjectFile(isEditor ? desc.ProjectFile : cookedRoot / "Project" / "Project.koi.toml");
			}

			// The project's [logging] table configures the logger before anything else logs
			AppCreateDesc appDesc = LoadProjectDesc(cmd->GetProjectFile());
			ResolveLogPath(appDesc.LogConfig, savedDir);
			ApplyLogOverrides(appDesc.LogConfig, cmd->GetLogLevels());
			Logger::Configure(appDesc.LogConfig);
			KOI_LOG(Engine, Info, "Koi {} ({})", KOI_VERSION, KOI_BUILD_MODE);
			KOI_LOG(Engine, Debug, "Project file: {}", cmd->GetProjectFile().string());

			if (isEditor)
			{
				MountSourceTree(desc);
			}
			else
			{
				MountCooked(cookedRoot);
			}

			Engine engine(Engine::Desc
			{
				.D3DDebug      = cmd->IsD3DDebugEnabled(),
				.GpuValidation = cmd->IsGpuValidationEnabled()
			});

			KOI_LOG(Engine, Debug, "Creating application instance");
			std::unique_ptr<AppBase> app = CreateApplicationInstance(*cmd);
			if (!app)
			{
				KOI_LOG(Engine, Critical, "Failed to create application");
				return -1;
			}

			app->SetDesc(std::move(appDesc));

			if (desc.Adapter)
			{
				// Editor build: wrap the game in an EditorApp supplied by the launcher
				KOI_LOG(Engine, Debug, "Host adapter present; wrapping application for the editor");
				LaunchDesc resolved = desc;
				resolved.ProjectFile = cmd->GetProjectFile();

				std::unique_ptr<IAppHost> host = desc.Adapter(std::move(app), resolved);
				if (!host)
				{
					KOI_LOG(Engine, Critical, "Host adapter returned null");
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
			KOI_LOG(Engine, Critical, "Unhandled exception: {}\n{}", e.what(), CrashHandler::Format(CrashHandler::TraceOf(e)));
			return 1;
		}
		catch (...)
		{
			KOI_LOG(Engine, Critical, "Unhandled unknown exception\n{}", CrashHandler::Format(CrashHandler::LastThrowTrace()));
			return 2;
		}
	}

	int RunApplication(int argc, wchar_t* wargv[], const LaunchDesc& desc)
	{
		std::vector<std::string> args(argc);
		std::vector<char*> argv(argc);
		for (int i = 0; i < argc; ++i)
		{
			args[i] = utils::FromWide(wargv[i]);
			argv[i] = args[i].data();
		}
		return RunApplication(argc, argv.data(), desc);
	}
}
