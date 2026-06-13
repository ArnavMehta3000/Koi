#pragma once
#include "Core/Types.h"
#include "Runtime/EngineHooks.h"
#include <filesystem>
#include <string>

namespace koi::editor
{
	namespace fs = std::filesystem;

	struct EditorConfig
	{
		fs::path EngineSource;   // <engine>/Assets
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // explicit --project file
		fs::path CookedOutput;   // where Cook writes (defaults to <exe>/Cooked)
	};

	class EditorApp : public NonCopyable
	{
	public:
		EditorApp();
		~EditorApp();

		void Initialize(EditorConfig config);
		void Shutdown();

	private:
		void OnPostInit();
		void OnPostRender();
		void DrawDockspace();
		void DrawMenuBar();
		void Cook();

		EditorConfig m_config{};
		bool         m_initialized      = false;
		HookId       m_postInitHook     = INVALID_HOOK_ID;
		HookId       m_postRenderHook   = INVALID_HOOK_ID;
		bool         m_imguiInitialized = false;
		std::string  m_lastCookStatus;  // shown in menubar after Cook click
	};
}
