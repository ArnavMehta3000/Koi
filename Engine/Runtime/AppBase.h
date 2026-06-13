#pragma once
#include "Platform/Window.h"
#include "Core/Logger.h"
#include "Runtime/CommandLine.h"

namespace koi
{
	class Engine;

	// Platform-agnostic depth buffer format option for AppCreateDesc.
	// Maps to the corresponding Format::D* value internally.
	enum class DepthBufferFormat : u8
	{
		None,   // No depth buffer
		D16,    // D16_UNORM
		D32,    // D32_FLOAT (default)
		D24S8,  // D24_UNORM_S8_UINT
	};

	struct AppCreateDesc
	{
		WindowDesc        WindowDesc{};
		LoggerConfig      LogConfig{};
		std::string       AppName      = "Koi Application";
		std::string       StartupScene;  // path relative to executable, empty = none
		DepthBufferFormat DepthBuffer  = DepthBufferFormat::D32;
	};

	class AppBase : public NonCopyable
	{
		friend class Engine;
	public:
		explicit AppBase(const AppCreateDesc& desc);
		virtual ~AppBase();

		[[nodiscard]] static AppBase& Get() { return *s_instance; }
		[[nodiscard]] const AppCreateDesc& GetDesc() const { return m_desc; }
		[[nodiscard]] Engine* GetEngine() const { return m_engine; }
		[[nodiscard]] Window& GetWindow() const { assert(m_window); return *m_window; }

		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnTick(f32 /*dt*/) {}
		virtual void OnRender() {}

	private:
		void Init();       // called by Engine::RunApp
		void Shutdown();   // called by Engine::RunApp

		AppCreateDesc m_desc;
		Engine* m_engine = nullptr;
		Window* m_window = nullptr;

		static inline AppBase* s_instance = nullptr;
	};

	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd);

	namespace internal
	{
		int RunApplication(int argc, char* argv[]);
	}
}
