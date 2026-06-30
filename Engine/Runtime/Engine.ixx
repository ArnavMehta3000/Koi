module;
#include <array>
#include <optional>
#include <vector>
export module Koi.Runtime.Engine;

import Koi.Core.Types;
import Koi.Platform.Window;
import Koi.Platform.InputManager;
import Koi.Graphics.Renderer;
import Koi.Memory.MemoryTracker;
import Koi.Runtime.Timer;
import Koi.Runtime.EngineHooks;

export namespace koi
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

		[[nodiscard]] Window&       GetWindow()           { return *m_window;                   }
		[[nodiscard]] const Window& GetWindow()     const { return *m_window;                   }
		[[nodiscard]] Renderer&     GetRenderer()         { return *m_renderer;                 }
		[[nodiscard]] InputManager& GetInputManager()     { return *m_inputManager;             }
		[[nodiscard]] f32           GetDeltaTime()  const { return m_timer.GetDeltaTime();      }
		[[nodiscard]] f64           GetTime()       const { return m_timer.GetTotalTime();      }
		[[nodiscard]] u64           GetFrameCount() const { return m_timer.GetFrameCount();     }
		[[nodiscard]] Timer&        GetTimer()            { return m_timer;                     }

		HookId RegisterHook(EnginePhase phase, EngineHook callback);
		void   UnregisterHook(HookId id);
		void   FireHook(EnginePhase phase);

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

		HooksArray                    m_hooks;
		HookId                        m_nextHookId = 1;
		MemTagSnapshotArray           m_initSnapshot{};
		std::optional<Window>         m_window;
		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<Renderer>     m_renderer;
		Timer                         m_timer;
		bool                          m_isRunning = true;
	};
}
