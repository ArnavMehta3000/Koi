#pragma once
#include "Koi/Engine/Core/Core.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Timer.h"
#include "Koi/Engine/Interop/AppHost.h"
#include <optional>

namespace koi
{
	class Engine : public NonCopyable
	{
	public:
		struct Stats
		{
			f32 DeltaTime  = 0.0f;
			u32 FPS        = 0;
			u64 FrameCount = 0;
			f64 TotalTime  = 0.0;
		};

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
		[[nodiscard]] inline AssetRegistry&       GetAssets()       { return m_assets;   }
		[[nodiscard]] inline const AssetRegistry& GetAssets() const { return m_assets;   }

		[[nodiscard]] Stats GetStats() const;

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
		AssetRegistry                 m_assets;

		Timer m_timer;
		bool  m_isRunning = true;
	};
}
