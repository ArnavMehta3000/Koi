#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Interop/RendererOverlay.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Interop/Reloadable.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Editor/Selection.h"
#include <toml++/toml.hpp>
#include <filesystem>

namespace koi
{
	class AppBase;
	class Engine;
	class CommandList;
}

namespace koi::editor
{
	namespace fs = std::filesystem;

	class AssetDatabase;
	class ScriptEditorPanel;
	class ViewportPanel;
	enum class Theme : u8;
	enum class ViewportCamera : u8;

	struct EditorConfig
	{
		fs::path EngineSource;   // <engine>/Assets
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // explicit --project file
		fs::path CookedOutput;   // where Cook writes (defaults to <exe>/Cooked)
	};

	class EditorApp
		: public NonCopyable
		, public IAppHost
		, public IRendererOverlay
		, public IEditorContext
	{
	public:
		EditorApp(std::unique_ptr<AppBase> game, EditorConfig config);
		~EditorApp() override;  // out of line: m_assetDatabase is forward-declared

		[[nodiscard]] const AppCreateDesc& GetDesc() const override;
		void SetWindow(Window& window) override;
		void HostInit() override;
		void HostTick(f32 dt) override;
		void HostRender() override;
		void HostShutdown() override;
		void HostBeginPlay() override;
		void HostEndPlay() override;

		void OnOverlayRender(CommandList& cmd) override;

		void RegisterReloadable(IReloadable* reloadable) override;
		void RegisterPanel(std::unique_ptr<IEditorPanel> panel) override;
		using IEditorContext::RegisterPanel;

		[[nodiscard]] std::optional<RenderView> GetEditorRenderView() const override;
		void OpenScriptEditor(std::string_view vfsPath) override;

	private:
		void Setup();
		void InitImGui();
		void ShutdownImGui();
		void DrawMenuBar();
		void ReloadAll(const ReloadRequest& request);
		void SaveScene();
		void Cook();

		void Play();
		void TogglePause();
		void Stop();

	private:
		std::unique_ptr<AppBase>                    m_game;
		std::unique_ptr<AssetDatabase>              m_assetDatabase;
		EditorConfig                                m_config{};
		std::vector<std::unique_ptr<IEditorPanel>>  m_panels;
		std::vector<bool>                           m_panelOpen;  // parallel to m_panels
		std::vector<IReloadable*>                   m_reloadables;
		ViewportPanel*                              m_viewportPanel     = nullptr;
		ScriptEditorPanel*                          m_scriptEditorPanel = nullptr;
		Selection                                   m_selection{};
		Theme                                       m_theme;
		bool                                        m_imguiInitialized = false;
		std::string                                 m_lastCookStatus;  // shown in menubar after Cook click

		toml::table                                 m_snapshot;            // the world as it was when Play was pressed
		std::vector<MeshHandle>                     m_pinnedAssets;        // keeps the snapshot's assets resident across Stop
		ViewportCamera                              m_cameraBeforePlay{};  // restored on Stop
	};
}
