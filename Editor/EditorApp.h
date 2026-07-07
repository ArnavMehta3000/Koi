#pragma once
#include "Core/Types.h"
#include "Editor/AppHost.h"
#include "Editor/RendererOverlay.h"
#include "Editor/EditorContext.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace koi
{
	class AppBase;
	class Engine;
	class CommandList;
	class ViewportPanel;
	struct Camera;
}

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

	class EditorApp
		: public NonCopyable
		, public IAppHost
		, public IRendererOverlay
		, public IEditorContext
	{
	public:
		EditorApp(std::unique_ptr<AppBase> game, EditorConfig config);
		~EditorApp() override = default;

		[[nodiscard]] const AppCreateDesc& GetDesc() const override;
		void SetWindow(Window& window) override;
		void HostInit() override;
		void HostTick(f32 dt) override;
		void HostRender() override;
		void HostShutdown() override;

		void OnOverlayRender(CommandList& cmd) override;

		void RegisterReloadable(IReloadable* reloadable) override;
		void RegisterPanel(std::unique_ptr<IEditorPanel> panel) override;
		using IEditorContext::RegisterPanel;

		[[nodiscard]] const ViewportTarget* GetViewportTarget() const override;
		[[nodiscard]] const Camera* GetEditorCamera() const override;

	private:
		void Setup();
		void InitImGui();
		void ShutdownImGui();
		void DrawMenuBar();
		void ReloadAll();
		void Cook();

	private:
		std::unique_ptr<AppBase>                    m_game;
		EditorConfig                                m_config{};
		std::vector<std::unique_ptr<IEditorPanel>>  m_panels;
		std::vector<IReloadable*>                   m_reloadables;
		ViewportPanel*                              m_viewportPanel    = nullptr;
		bool                                        m_imguiInitialized = false;
		std::string                                 m_lastCookStatus;  // shown in menubar after Cook click
	};
}
