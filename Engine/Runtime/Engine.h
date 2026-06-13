#pragma once
#include "Core/Core.h"
#include "Platform/Window.h"
#include "Platform/InputManager.h"
#include "Graphics/Renderer.h"
#include "Memory/MemoryTracker.h"
#include "Runtime/Timer.h"
#include "Runtime/EngineHooks.h"
#include <array>
#include <optional>
#include <vector>

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

		[[nodiscard]] inline Window&       GetWindow()          { return *m_window;        }
		[[nodiscard]] inline const Window& GetWindow() const    { return *m_window;        }
		[[nodiscard]] inline Renderer&     GetRenderer()        { return *m_renderer;      }
		[[nodiscard]] inline InputManager& GetInputManager()    { return *m_inputManager;  }
		[[nodiscard]] inline f32           GetDeltaTime() const { return m_timer.GetDeltaTime();  }
		[[nodiscard]] inline f64           GetTime() const      { return m_timer.GetTotalTime();  }
		[[nodiscard]] inline u64           GetFrameCount() const{ return m_timer.GetFrameCount(); }
		[[nodiscard]] inline Timer&        GetTimer()           { return m_timer;                 }

		HookId RegisterHook(EnginePhase phase, EngineHook callback);
		void UnregisterHook(HookId id);

		// Internal (public because Renderer fires it)
		void FireHook(EnginePhase phase);

	private:
		void BeginFrame();
		void Tick(AppBase& app);
		void EndFrame();
		void MainLoop(AppBase& app);

	private:
		struct HookEntry { HookId Id; EngineHook Callback; };
		using HooksArray = std::array<std::vector<HookEntry>, static_cast<size_t>(EnginePhase::MAX_COUNT)>;

	private:
		static inline Engine* s_instance = nullptr;

		HooksArray m_hooks;
		HookId     m_nextHookId = 1;

		MemTagSnapshotArray           m_initSnapshot{};
		std::optional<Window>         m_window;
		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<Renderer>     m_renderer;

		Timer m_timer;
		bool  m_isRunning = true;
	};
}
