#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Runtime/Timer.h"
#include <memory>
#include <string_view>

namespace koi
{
	class AssetRegistry;
	class IAppHost;
	class InputManager;
	class Renderer;
	class Window;

	enum class PlayState : u8
	{
		Edit,
		Playing,
		Paused,
	};

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

		[[nodiscard]] static constexpr std::string_view ToString(PlayState state)
		{
			switch (state)
			{
			case PlayState::Edit:    return "Edit";
			case PlayState::Playing: return "Playing";
			case PlayState::Paused:  return "Paused";
			default:                 return "<?>";
			}
		}

		void RunApp(IAppHost& host);
		void RequestExit() { m_isRunning = false; }

		[[nodiscard]] inline Window&       GetWindow()       { return *m_window; }
		[[nodiscard]] inline const Window& GetWindow() const { return *m_window; }

		// Instantiated in Engine.cpp for each subsystem; include the subsystem's own header to use it
		template <typename T> [[nodiscard]] T& GetSubsystem();

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

		struct Subsystems;

		std::unique_ptr<Window>     m_window;
		std::unique_ptr<Subsystems> m_systems;

		IAppHost* m_host = nullptr;  // valid for the duration of RunApp

		Timer     m_timer;
		bool      m_isRunning = true;
		PlayState m_playState = PlayState::Edit;
	};

	extern template InputManager&  Engine::GetSubsystem<InputManager>();
	extern template Renderer&      Engine::GetSubsystem<Renderer>();
	extern template AssetRegistry& Engine::GetSubsystem<AssetRegistry>();
}
