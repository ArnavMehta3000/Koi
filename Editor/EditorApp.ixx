module;
#include <filesystem>
#include <string>
export module Koi.Editor.App;

import Koi.Core.Types;
import Koi.Runtime.EngineHooks;

// Non-exported — visible in implementation units, not to importers.
import Koi.Graphics.Types;
import Koi.Graphics.RHI.Device;
import Koi.Graphics.RHI.CommandList;
import Koi.Graphics.ShaderCompiler;
import Koi.Graphics.ShaderLoader;
import Koi.Graphics.Renderer;
import Koi.Platform.Window;
import Koi.Platform.InputManager;
import Koi.Platform.InputCodes;
import Koi.Runtime.Engine;
import Koi.Memory.MemoryTracker;

export namespace koi::editor
{
	namespace fs = std::filesystem;

	struct EditorConfig
	{
		fs::path EngineSource;
		fs::path ProjectSource;
		fs::path ProjectFile;
		fs::path CookedOutput;
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
		void DrawMenuBar();
		void Cook();

		EditorConfig m_config{};
		bool         m_initialized      = false;
		HookId       m_postInitHook     = INVALID_HOOK_ID;
		HookId       m_postRenderHook   = INVALID_HOOK_ID;
		bool         m_imguiInitialized = false;
		std::string  m_lastCookStatus;
	};
}
