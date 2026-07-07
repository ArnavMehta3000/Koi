#pragma once
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/CommandLine.h"
#include "Koi/Engine/Interop/AppHost.h"
#include "Koi/Engine/Interop/ViewportTarget.h"
#include <optional>

namespace koi
{
	class Engine;
	class Scene;
	class IEditorContext;
	struct Camera;

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

	class AppBase : public NonCopyable, public IAppHost
	{
	public:
		explicit AppBase(const AppCreateDesc& desc);
		virtual ~AppBase();

		[[nodiscard]] static AppBase& Get() { return *s_instance; }
		[[nodiscard]] const AppCreateDesc& GetDesc() const override { return m_desc; }
		[[nodiscard]] Engine* GetEngine() const { return m_engine; }
		[[nodiscard]] Window& GetWindow() const { assert(m_window); return *m_window; }

		void SetWindow(Window& window) override { m_window = &window; }

		void HostInit() override;
		void HostTick(f32 dt) override;
		void HostRender() override;
		void HostShutdown() override;

		virtual void OnInit() {}
		virtual void OnShutdown() {}
		virtual void OnTick(f32 /*dt*/) {}
		virtual void OnRender() {}

		[[nodiscard]] IEditorContext* GetEditorContext() const { return m_editorContext; }
		void SetEditorContext(IEditorContext* context) { m_editorContext = context; }

		[[nodiscard]] virtual Scene* GetEditorScene() { return nullptr; }

		[[nodiscard]] std::optional<ViewportTarget> GetFrameTarget() const;
		[[nodiscard]] const Camera* ResolveCamera(const Camera* fallback) const;

	private:
		AppCreateDesc   m_desc;
		Engine*         m_engine        = nullptr;
		Window*         m_window        = nullptr;
		IEditorContext* m_editorContext = nullptr;

		static inline AppBase* s_instance = nullptr;
	};

	std::unique_ptr<AppBase> CreateApplication(CommandLine& cmd);

	namespace internal
	{
		int RunApplication(int argc, char* argv[]);
	}
}
