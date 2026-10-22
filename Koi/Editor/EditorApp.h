#pragma once
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"
#include "Koi/Editor/ImGui/ImGuiLayer.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Panels/PanelHost.h"
#include "Koi/Editor/Selection.h"
#include "Koi/Editor/StatusBar.h"
#include "Koi/Editor/Toasts.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Graphics/RendererOverlay.h"
#include "Koi/Engine/Graphics/RenderView.h"
#include "Koi/Engine/Runtime/AppHost.h"
#include "Koi/Engine/Runtime/Reloadable.h"
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
	class LogPanel;
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

		[[nodiscard]] AppBase* GetGame() const override { return m_game.get(); }
		void RegisterReloadable(IReloadable* reloadable) override;
		void RegisterPanel(std::unique_ptr<IEditorPanel> panel) override;
		void RegisterInspector(std::unique_ptr<IComponentInspector> inspector) override;
		using IEditorContext::RegisterPanel;
		using IEditorContext::RegisterInspector;

	private:
		void Setup();
		void StartExtensions();
		void ShutdownExtensions();
		[[nodiscard]] std::optional<RenderView> GetViewportView() const;

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
		void ReportPlayFailure(const std::exception& e);

	private:
		std::unique_ptr<AppBase>        m_game;
		std::unique_ptr<AssetDatabase>  m_assetDatabase;
		std::unique_ptr<ImportQueue>    m_importQueue;
		ImportPanel*                    m_importPanel = nullptr;
		LogPanel*                       m_logPanel    = nullptr;
		EditorConfig                    m_config{};
		ImGuiLayer                      m_imgui;
		PanelHost                       m_panels;
		StatusBar                       m_statusBar;
		Toasts                          m_toasts;
		InspectorRegistry               m_inspectors;
		std::vector<IReloadable*>       m_reloadables;
		std::vector<std::unique_ptr<EditorExtension>> m_extensions;
		ViewportPanel*                  m_viewportPanel     = nullptr;
		Selection                       m_selection{};
		GizmoOptions                    m_gizmos{};
		std::optional<ReloadRequest>    m_pendingReload;

		toml::table                     m_snapshot;            // the world as it was when Play was pressed
		std::vector<MeshHandle>         m_pinnedMeshes;        // keep the snapshot's assets resident across Stop
		std::vector<MaterialHandle>     m_pinnedMaterials;
		ViewportCamera                  m_cameraBeforePlay{};  // restored on Stop
	};
}
