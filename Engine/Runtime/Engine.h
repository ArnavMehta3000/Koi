#pragma once
#include "Core/Core.h"
#include "Platform/WindowManager.h"
#include "Platform/InputManager.h"
#include "Graphics/Renderer.h"
#include "Memory/MemoryTracker.h"
#include "Runtime/Timer.h"

namespace koi
{
	class AppBase;

	class Engine : public NonCopyable
	{
	public:
		Engine();
		~Engine();

		[[nodiscard]] static Engine& Get() { return *s_instance; }

		void RunApp(AppBase& app);
		void RequestExit() { m_isRunning = false; }

		[[nodiscard]] inline WindowManager&     GetWindowManager()                { return *m_windowManager;   }
		[[nodiscard]] inline Renderer&          GetRenderer()                     { return *m_renderer;        }
		[[nodiscard]] inline InputManager&      GetInputManager()                 { return *m_inputManager;    }
		[[nodiscard]] inline f32                GetDeltaTime() const              { return m_timer.GetDeltaTime();  }
		[[nodiscard]] inline f64                GetTime() const                   { return m_timer.GetTotalTime();  }
		[[nodiscard]] inline u64                GetFrameCount() const             { return m_timer.GetFrameCount(); }
		[[nodiscard]] inline Timer&             GetTimer()                        { return m_timer;                 }
		[[nodiscard]] inline RenderWindowHandle GetMainRenderWindowHandle() const { return m_mainRenderWindow;      }

		std::pair<WindowHandle, RenderWindowHandle> CreateAndAddRenderWindow(const WindowDesc& desc);
		RenderWindowHandle RegisterWindowWithRenderer(WindowHandle handle, bool isVsync);

	private:
		void BeginFrame();
		void Tick(AppBase& app);
		void EndFrame();
		void MainLoop(AppBase& app);

	private:
		static inline Engine* s_instance = nullptr;

		MemTagSnapshotArray            m_initSnapshot{};
		std::unique_ptr<WindowManager> m_windowManager;
		std::unique_ptr<InputManager>  m_inputManager;
		std::unique_ptr<Renderer>      m_renderer;

		Timer              m_timer;
		bool               m_isRunning  = true;
		RenderWindowHandle m_mainRenderWindow;
	};
}
