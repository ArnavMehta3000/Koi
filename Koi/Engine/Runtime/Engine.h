#pragma once
#include "Koi/Engine/Core/Core.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Timer.h"
#include "Koi/Engine/Runtime/Subsystems.h"
#include "Koi/Engine/Interop/AppHost.h"
#include <optional>

namespace koi
{
	enum class PlayState : u8
	{
		Edit,
		Playing,
		Paused,
	};

	constexpr std::string_view ToString(PlayState state)
	{
		switch (state)
		{
		case PlayState::Edit:    return "Edit";
		case PlayState::Playing: return "Playing";
		case PlayState::Paused:  return "Paused";
		default:                 return "<?>";
		}
	}

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

		[[nodiscard]] inline Window&       GetWindow()       { return *m_window; }
		[[nodiscard]] inline const Window& GetWindow() const { return *m_window; }

		template <typename T> [[nodiscard]] inline T& GetSubsystem() { return m_systems.Get<T>(); }
		template <typename T> [[nodiscard]] inline const T& GetSubsystem() const { return m_systems.Get<T>(); }

		[[nodiscard]] Stats GetStats() const;

		void SetPlayState(PlayState state);
		[[nodiscard]] inline PlayState GetPlayState() const { return m_playState; }
		[[nodiscard]] inline bool IsPlaying() const { return m_playState == PlayState::Playing; }

	private:
		void BeginFrame();
		void Tick(IAppHost& host);
		void EndFrame();
		void MainLoop(IAppHost& host);

	private:
		static inline Engine* s_instance = nullptr;

		// Declaration order is init order | shutdown runs in reverse
		using Subsystems = SubsystemSet<InputManager, Renderer, AssetRegistry>;

		MemTagSnapshotArray   m_initSnapshot{};
		std::optional<Window> m_window;
		Subsystems            m_systems;

		IAppHost* m_host = nullptr;  // valid for the duration of RunApp

		Timer     m_timer;
		bool      m_isRunning = true;
		PlayState m_playState = PlayState::Edit;
	};
}
