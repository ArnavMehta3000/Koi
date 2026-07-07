#pragma once
#include "Koi/Engine/Core/Core.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Timer.h"
#include "Koi/Engine/Interop/AppHost.h"
#include <optional>

namespace koi
{
	class Engine : public NonCopyable
	{
	public:
		Engine();
		~Engine();

		[[nodiscard]] static Engine& Get() { return *s_instance; }

		void RunApp(IAppHost& host);
		void RequestExit() { m_isRunning = false; }

		[[nodiscard]] inline Window&       GetWindow()          { return *m_window;        }
		[[nodiscard]] inline const Window& GetWindow() const    { return *m_window;        }
		[[nodiscard]] inline Renderer&     GetRenderer()        { return *m_renderer;      }
		[[nodiscard]] inline InputManager& GetInputManager()    { return *m_inputManager;  }
		[[nodiscard]] inline f32           GetDeltaTime() const { return m_timer.GetDeltaTime();  }
		[[nodiscard]] inline f64           GetTime() const      { return m_timer.GetTotalTime();  }
		[[nodiscard]] inline u64           GetFrameCount() const{ return m_timer.GetFrameCount(); }
		[[nodiscard]] inline Timer&        GetTimer()           { return m_timer;                 }

	private:
		void BeginFrame();
		void Tick(IAppHost& host);
		void EndFrame();
		void MainLoop(IAppHost& host);

	private:
		static inline Engine* s_instance = nullptr;

		MemTagSnapshotArray           m_initSnapshot{};
		std::optional<Window>         m_window;
		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<Renderer>     m_renderer;

		Timer m_timer;
		bool  m_isRunning = true;
	};
}
