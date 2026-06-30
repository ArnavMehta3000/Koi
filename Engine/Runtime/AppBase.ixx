module;
#include <string>
#include <memory>
#include <cassert>
export module Koi.Runtime.AppBase;

import Koi.Core.Logger;
import Koi.Platform.Window;
import Koi.Runtime.CommandLine;

export namespace koi
{
	class Engine;

	enum class DepthBufferFormat : u8
	{
		None,
		D16,
		D32,
		D24S8,
	};

	struct AppCreateDesc
	{
		WindowDesc        WindowDesc{};
		LoggerConfig      LogConfig{};
		std::string       AppName      = "Koi Application";
		std::string       StartupScene;
		DepthBufferFormat DepthBuffer  = DepthBufferFormat::D32;
	};

	class AppBase : public NonCopyable
	{
		friend class Engine;
	public:
		explicit AppBase(const AppCreateDesc& desc);
		virtual ~AppBase();

		[[nodiscard]] static AppBase& Get()                  { return *s_instance; }
		[[nodiscard]] const AppCreateDesc& GetDesc() const   { return m_desc;      }
		[[nodiscard]] Engine* GetEngine() const              { return m_engine;    }
		[[nodiscard]] Window& GetWindow() const              { assert(m_window); return *m_window; }

		virtual void OnInit()           {}
		virtual void OnShutdown()       {}
		virtual void OnTick(f32 /*dt*/) {}
		virtual void OnRender()         {}

	private:
		void Init();
		void Shutdown();

		AppCreateDesc m_desc;
		Engine*       m_engine = nullptr;
		Window*       m_window = nullptr;

		static inline AppBase* s_instance = nullptr;
	};

	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd);

	namespace internal
	{
		int RunApplication(int argc, char* argv[]);
	}
}
