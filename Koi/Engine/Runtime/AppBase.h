#pragma once
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Graphics/RenderView.h"
#include "Koi/Engine/Logging/LoggerConfig.h"
#include "Koi/Engine/Platform/InputActions.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/AppHost.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace koi
{
	class CommandLine;
	class Engine;
	class World;

	struct AppCreateDesc
	{
		WindowDesc   WindowDesc{};
		LoggerConfig LogConfig{};
		InputConfig  Input{};
		std::string  AppName        = "Koi Application";
		std::map<std::string, std::string> Scenes;   // name -> virtual path, from [scenes.list]
		std::string  StartupScene;                   // name in Scenes, empty = none
		f32          FixedStep      = 1.0f / 60.0f;  // seconds per FixedUpdate step, from [physics]
	};

	class AppBase : public NonCopyable, public IAppHost
	{
	public:
		AppBase();
		virtual ~AppBase();

		// Defined by the game. Runs before the project TOML is applied
		[[nodiscard]] static std::unique_ptr<AppBase> Create(CommandLine& cmd);

		[[nodiscard]] static AppBase& Get() { return *s_instance; }
		[[nodiscard]] const AppCreateDesc& GetDesc() const override { return m_desc; }
		[[nodiscard]] Engine* GetEngine() const { return m_engine; }
		[[nodiscard]] Window& GetWindow() const { KOI_ASSERT(m_window); return *m_window; }
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

		// Both run before the startup scene loads; register with ComponentRegistry / SystemRegistry
		virtual void OnRegisterComponents() {}
		virtual void OnRegisterSystems() {}

		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnTick(f32 /*dt*/) {}
		virtual void OnRender() {}
		virtual void OnBeginPlay() {}
		virtual void OnEndPlay() {}

		void HostRenderInto(const std::optional<RenderView>& view);
		[[nodiscard]] std::optional<RenderView> GetWindowView() const;

		virtual void OnSceneLoaded(std::string_view /*name*/) {}
		bool LoadScene(std::string_view name);

	private:
		void LoadStartupScene();
		void LoadSceneNow(const std::string& name);

	private:
		AppCreateDesc          m_desc;
		std::string            m_pendingScene;
		std::unique_ptr<World> m_world;
		Engine*                m_engine = nullptr;
		Window*                m_window = nullptr;

		static inline AppBase* s_instance = nullptr;
	};
}
