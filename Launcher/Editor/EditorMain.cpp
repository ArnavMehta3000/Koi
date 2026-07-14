#include "Koi/Engine/Runtime/EntryPoint.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Text/StringUtils.h"
#include "Koi/Editor/EditorApp.h"
#include <filesystem>
#include <vector>
#include <string>

#if KOI_DEBUG
	#include <ranges>
#endif

namespace koi::editor::internal
{
	namespace fs = std::filesystem;

	static int RunEditor(int argc, char* argv[])
	{
		Logger::Init();

		try
		{
			KOI_LOG_INFO("Starting Koi Editor");

			auto cmd = koi::internal::InvokeCommandLineFactory();
			if (cmd->ParseArgs(argc, argv) != 0)
			{
				KOI_LOG_CRITICAL("Failed to parse command line arguments");
				return -1;
			}

			// Setup VFS mounts
			{
				KOI_LOG_DEBUG("Setting up editor VFS mounts");
				const fs::path exeDir = fs::current_path();

				KOI_LOG_DEBUG("Current working directory {}", exeDir.string());

				std::vector<fs::path> engineRoots;
				if (!cmd->GetEngineSource().empty())
					engineRoots.push_back(fs::absolute(cmd->GetEngineSource()));

				engineRoots.push_back(exeDir / "Engine");

#if KOI_DEBUG
				KOI_LOG_DEBUG("Configured engine roots:");
				for (const auto& [i, path] : std::views::enumerate(engineRoots) | std::views::as_const)
				{
					KOI_LOG_DEBUG("\t{}: {}", i, path.string());
				}
#endif

				std::vector<fs::path> projectRoots;
				if (!cmd->GetProjectSource().empty())
					projectRoots.push_back(fs::absolute(cmd->GetProjectSource()));

				projectRoots.push_back(exeDir);

#if KOI_DEBUG
				KOI_LOG_DEBUG("Configured project roots:");
				for (const auto& [i, path] : std::views::enumerate(projectRoots) | std::views::as_const)
				{
					KOI_LOG_DEBUG("\t{} : {}", i, path.string());
				}
#endif

				VirtualFileSystem& vfs = VirtualFileSystem::Get();
				vfs.Mount("engine", std::make_unique<FilesystemMount>(engineRoots));
				vfs.Mount("project", std::make_unique<FilesystemMount>(projectRoots));
			}

			// Engine/Editor scope
			{
				const EditorConfig editorCfg
				{
					.EngineSource = cmd->GetEngineSource().empty()
									   ? fs::path{}
									   : fs::absolute(cmd->GetEngineSource()),
					.ProjectSource = cmd->GetProjectSource().empty()
									   ? fs::path{}
									   : fs::absolute(cmd->GetProjectSource()),
					.ProjectFile = fs::absolute(cmd->GetProjectFile()),
					.CookedOutput = fs::current_path() / "Cooked",
				};

				// Create the engine and run the editor app
				Engine engine;
				std::unique_ptr<AppBase> gameApp = CreateApplication(*cmd);

				if (!gameApp)
				{
					KOI_LOG_CRITICAL("Failed to create application");
					return -1;
				}

				EditorApp editorApp(std::move(gameApp), editorCfg);
				engine.RunApp(editorApp);
			}
			return 0;
		}
		catch (const std::exception& e)
		{
			KOI_LOG_CRITICAL("Unhandled exception: {}", e.what());
			Logger::Shutdown();
			return 1;
		}
	}
}

#if KOI_DEBUG
int main(int argc, char* argv[])
{
	return koi::editor::internal::RunEditor(argc, argv);
}
#else
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	std::vector<std::string> args(__argc);
	std::vector<char*> argv(__argc);
	for (int i = 0; i < __argc; ++i)
	{
		args[i] = koi::StringUtils::FromWide(__wargv[i]);
		argv[i] = args[i].data();
	}
	return koi::editor::internal::RunEditor(__argc, argv.data());
}
#endif
