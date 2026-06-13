#include "Runtime/EntryPoint.h"
#include "Runtime/Engine.h"
#include "Runtime/AppBase.h"
#include "Core/VirtualFileSystem.h"
#include "Core/StringUtils.h"
#include "EditorApp.h"
#include <filesystem>
#include <vector>
#include <string>

#if KOI_DEBUG
	#include <ranges>
#endif

// koi-editor: same engine + game lifecycle as koi-runtime, with the
// editor subsystem (ImGui overlay, hot-reload, future tools) attached as
// hooks. The game runs as the only AppBase — the editor is NOT a wrapper
// app; it sits alongside the game and broadcasts via engine hooks.

namespace koi::internal_editor
{
	namespace fs = std::filesystem;

	static int RunEditor(int argc, char* argv[])
	{
		assert(argc > 0 && argv);

		Logger::Init();

		try
		{
			auto cmd = internal::InvokeCommandLineFactory();
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
					KOI_LOG_DEBUG("\t{} : {}", i, path.string());
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

			Engine engine;
			editor::EditorApp editorApp;
			editor::EditorConfig editorCfg
			{
				.EngineSource  = cmd->GetEngineSource().empty()
				                   ? fs::path{}
				                   : fs::absolute(cmd->GetEngineSource()),
				.ProjectSource = cmd->GetProjectSource().empty()
				                   ? fs::path{}
				                   : fs::absolute(cmd->GetProjectSource()),
				.ProjectFile   = fs::absolute(cmd->GetProjectFile()),
				.CookedOutput  = fs::current_path() / "Cooked",
			};
			editorApp.Initialize(std::move(editorCfg));

			if (auto app = CreateApplication(*cmd))
			{
				engine.RunApp(*app);
				return 0;
			}

			KOI_LOG_CRITICAL("Failed to create application");
			return -1;
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
	return koi::internal_editor::RunEditor(argc, argv);
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
	return koi::internal_editor::RunEditor(__argc, argv.data());
}
#endif
