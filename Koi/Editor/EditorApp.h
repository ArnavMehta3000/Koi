#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Interop/RendererOverlay.h"
#include "Koi/Engine/Interop/EditorContext.h"
#include "Koi/Engine/Interop/Reloadable.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Editor/ImGui/ImGuiLayer.h"
#include "Koi/Editor/Inspectors/InspectorRegistry.h"
#include "Koi/Editor/Panels/PanelHost.h"
#include "Koi/Editor/Selection.h"
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
	class ViewportPanel;
	enum class ViewportCamera : u8;

	struct EditorConfig
	{
		fs::path EngineSource;   // <engine>/Assets
		fs::path ProjectSource;  // game source root (contains the .toml)
		fs::path ProjectFile;    // explicit --project file
		fs::path CookedOutput;   // where Cook writes (defaults to <exe>/Cooked)
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

		void Play();
		void TogglePause();
		void Stop();

	private:
		std::unique_ptr<AppBase>        m_game;
		std::unique_ptr<AssetDatabase>  m_assetDatabase;
		EditorConfig                    m_config{};
		ImGuiLayer                      m_imgui;
		PanelHost                       m_panels;
		InspectorRegistry               m_inspectors;
		std::vector<IReloadable*>       m_reloadables;
		ViewportPanel*                  m_viewportPanel     = nullptr;
		Selection                       m_selection{};
		GizmoOptions                    m_gizmos{};
		std::string                     m_lastCookStatus;  // shown in menubar after Cook click
		std::optional<ReloadRequest>    m_pendingReload;

		toml::table                     m_snapshot;            // the world as it was when Play was pressed
		std::vector<MeshHandle>         m_pinnedAssets;        // keeps the snapshot's assets resident across Stop
		ViewportCamera                  m_cameraBeforePlay{};  // restored on Stop
	};
}
