#pragma once
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include <cassert>
#include <memory>
#include <optional>

namespace koi
{
	class CommandLine;
	class Engine;
	class IEditorContext;
	class World;

	struct AppCreateDesc
	{
		WindowDesc   WindowDesc{};
		LoggerConfig LogConfig{};
		std::string  AppName        = "Koi Application";
		std::string  StartupScene;  // path relative to executable, empty = none
	};

	class AppBase : public NonCopyable, public IAppHost
	{
	public:
		AppBase();
		virtual ~AppBase();

		[[nodiscard]] static AppBase& Get() { return *s_instance; }
		[[nodiscard]] const AppCreateDesc& GetDesc() const override { return m_desc; }
		[[nodiscard]] Engine* GetEngine() const { return m_engine; }
		[[nodiscard]] Window& GetWindow() const { assert(m_window); return *m_window; }
		[[nodiscard]] World& GetWorld() const { return *m_world; }

		// Filled from the project TOML by RunApplication, before the engine starts
		inline void SetDesc(AppCreateDesc desc) { m_desc = std::move(desc); }

		void SetWindow(Window& window) override { m_window = &window; }
		[[nodiscard]] bool StartsInPlayMode() const override { return true; }

		void HostInit() override;
		void HostTick(f32 dt) override;
		void HostRender() override;
		void HostShutdown() override;
		void HostBeginPlay() override;
		void HostEndPlay() override;

		virtual void OnRegisterComponents() {}  // Runs before the startup scene loads

		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnTick(f32 /*dt*/) {}
		virtual void OnRender() {}
		virtual void OnBeginPlay() {}
		virtual void OnEndPlay() {}

		[[nodiscard]] IEditorContext* GetEditorContext() const { return m_editorContext; }
		void SetEditorContext(IEditorContext* context) { m_editorContext = context; }

		// Resolves the render target for this frame and the camera to view it through.
		// The target's aspect is only known here, so the world's camera is projected
		// against it rather than against the window
		[[nodiscard]] std::optional<RenderView> GetRenderView() const;

	private:
		void LoadStartupScene();

	private:
		AppCreateDesc          m_desc;
		std::unique_ptr<World> m_world;
		Engine*                m_engine        = nullptr;
		Window*                m_window        = nullptr;
		IEditorContext*        m_editorContext = nullptr;

		static inline AppBase* s_instance = nullptr;
	};

	// Runs before the project TOML is applied
	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd);

	namespace internal
	{
		int RunApplication(int argc, char* argv[]);
	}
}
