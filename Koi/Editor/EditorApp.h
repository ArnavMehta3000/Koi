#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Interop/RendererOverlay.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Interop/Reloadable.h"
#include "Koi/Editor/ImGui/ImGuiLayer.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Panels/PanelHost.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Editor/StatusBar.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <optional>

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
	class ImportPanel;
	class ImportQueue;
	class ViewportPanel;
	enum class ViewportCamera : u8;

	struct EditorConfig
	{
		fs::path EngineSource;   // <repo>/Koi/Engine
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // project TOML in use
		fs::path CookedOutput;   // <project>/.koi/Cooked
	};

	// For debug lines
	struct GizmoOptions
	{
		bool SelectionBounds = true;
		bool Lights          = true;
		bool CameraFrustum   = false;
		bool ShadowVolume    = false;
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
		[[nodiscard]] bool StartsInPlayMode() const override { return false; }
		void HostInit() override;
		void HostTick(f32 dt) override;
		void HostRender() override;
		void HostShutdown() override;
		void HostBeginPlay() override;
		void HostEndPlay() override;

		void OnOverlayRender(CommandList& cmd) override;

		void RegisterReloadable(IReloadable* reloadable) override;
		void RegisterPanel(std::unique_ptr<IEditorPanel> panel) override;
		void RegisterInspector(std::unique_ptr<IComponentInspector> inspector) override;
		using IEditorContext::RegisterPanel;
		using IEditorContext::RegisterInspector;

		[[nodiscard]] std::optional<RenderView> GetEditorRenderView() const override;

	private:
		void Setup();
		void DrawMenuBar();
		void DrawPlayControls();
		void DrawGizmoMenu();
		void PollShortcuts();
		void ReloadAll(const ReloadRequest& request);
		void SaveScene();
		void Cook();

		void QueueGizmos();
		void DrawSelectionBounds();
		void DrawLightGizmos();
		void DrawCameraFrustum();
		void DrawShadowVolume();

		// Per-project editor state (camera) in <project>/.koi/Editor.toml
		[[nodiscard]] fs::path EditorStatePath() const;
		bool LoadEditorState();
		void SaveEditorState() const;

		void Play();
		void TogglePause();
		void Stop();

	private:
		std::unique_ptr<AppBase>        m_game;
		std::unique_ptr<AssetDatabase>  m_assetDatabase;
		std::unique_ptr<ImportQueue>    m_importQueue;
		ImportPanel*                    m_importPanel = nullptr;
		EditorConfig                    m_config{};
		ImGuiLayer                      m_imgui;
		PanelHost                       m_panels;
		StatusBar                       m_statusBar;
		InspectorRegistry               m_inspectors;
		std::vector<IReloadable*>       m_reloadables;
		ViewportPanel*                  m_viewportPanel     = nullptr;
		Selection                       m_selection{};
		GizmoOptions                    m_gizmos{};
		std::string                     m_lastCookStatus;  // last Save/Cook result, shown in the status bar
		std::optional<ReloadRequest>    m_pendingReload;

		toml::table                     m_snapshot;            // the world as it was when Play was pressed
		std::vector<MeshHandle>         m_pinnedMeshes;        // keep the snapshot's assets resident across Stop
		std::vector<MaterialHandle>     m_pinnedMaterials;
		ViewportCamera                  m_cameraBeforePlay{};  // restored on Stop
	};
}
