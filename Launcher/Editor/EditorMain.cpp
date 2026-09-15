#include "Koi/Engine/Runtime/EntryPoint.h"
#include "Koi/Editor/EditorApp.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Runtime/AppBase.h"
#include <memory>

namespace
{
	std::unique_ptr<koi::IAppHost> MakeEditorHost(std::unique_ptr<koi::AppBase> game, const koi::internal::LaunchDesc& launch)
	{
		const koi::editor::EditorConfig config
		{
			.EngineSource  = launch.EngineDir,
			.ProjectSource = launch.ProjectDir,
			.ProjectFile   = launch.ProjectFile,
			.CookedOutput  = launch.ProjectDir / ".koi" / "Cooked",
		};
		return std::make_unique<koi::editor::EditorApp>(std::move(game), config);
	}

	// KOI_ENGINE_DIR comes from the generated config header, KOI_PROJECT_* from the KoiGame rule
	koi::internal::LaunchDesc Launch()
	{
		return
		{
			.EngineDir   = KOI_ENGINE_DIR,
			.ProjectDir  = KOI_PROJECT_DIR,
			.ProjectFile = KOI_PROJECT_FILE,
			.Adapter     = MakeEditorHost,
		};
	}
}

KOI_IMPLEMENT_MAIN_WITH(Launch)
