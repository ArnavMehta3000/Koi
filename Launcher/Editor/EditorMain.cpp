#include "Koi/Engine/Runtime/EntryPoint.h"
#include "Koi/Editor/EditorApp.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include <filesystem>
#include <memory>

namespace
{
	std::unique_ptr<koi::IAppHost> MakeEditorHost(std::unique_ptr<koi::AppBase> game, koi::CommandLine& cmd)
	{
		namespace fs = std::filesystem;
		const koi::editor::EditorConfig config
		{
			.EngineSource  = cmd.GetEngineSource().empty()  ? fs::path{} : fs::absolute(cmd.GetEngineSource()),
			.ProjectSource = cmd.GetProjectSource().empty() ? fs::path{} : fs::absolute(cmd.GetProjectSource()),
			.ProjectFile   = fs::absolute(cmd.GetProjectFile()),
			.CookedOutput  = fs::current_path() / "Cooked",
		};
		return std::make_unique<koi::editor::EditorApp>(std::move(game), config);
	}
}

// The created game AppBase is wrapped in an EditorApp before it is run (supplied as a host adapter)
KOI_IMPLEMENT_MAIN_WITH_HOST(MakeEditorHost)
