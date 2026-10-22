#pragma once
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/Core/EditorExtension.h"
#include "Koi/Editor/ImGui/ImGuiLayer.h"
#include "Koi/Editor/StatusBar.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Graphics/RendererOverlay.h"
#include "Koi/Engine/Graphics/RenderView.h"
#include "Koi/Engine/Runtime/AppHost.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace koi
{
	class AppBase;
	class Engine;
	class CommandList;
}

namespace koi::editor
{
	namespace fs = std::filesystem;

	class ImportPanel;
	class ImportQueue;
	class PanelHost;
	class PlaySession;
	class Reloader;
	class Toasts;
	class ViewportPanel;

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
	{
	public:
		EditorApp(std::unique_ptr<AppBase> game, EditorConfig config);
		~EditorApp() override;

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

	private:
		void Setup();
		void StartExtensions();
		void ShutdownExtensions();
		[[nodiscard]] std::optional<RenderView> GetViewportView() const;

		void DrawMenuBar();
		void DrawPlayControls();
		void DrawGizmoMenu();
		void PollShortcuts();
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

	private:
		using EdExtensions = std::vector<std::unique_ptr<EditorExtension>>;

		std::unique_ptr<AppBase> m_game;
		EditorConfig             m_config{};
		EditorContext            m_context;
		EdExtensions             m_extensions;
		ImGuiLayer               m_imgui;
		StatusBar                m_statusBar;
		GizmoOptions             m_gizmos{};

		// Owned by m_context, cached for access
		PanelHost*     m_panels        = nullptr;
		ImportQueue*   m_importQueue   = nullptr;
		PlaySession*   m_play          = nullptr;
		Reloader*      m_reloader      = nullptr;
		Toasts*        m_toasts        = nullptr;
		ImportPanel*   m_importPanel   = nullptr;
		ViewportPanel* m_viewportPanel = nullptr;
	};
}
