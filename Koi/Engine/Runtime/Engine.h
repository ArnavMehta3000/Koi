#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Runtime/Timer.h"
#include <filesystem>
#include <memory>
#include <string_view>

namespace koi
{
	namespace fs = std::filesystem;

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

		struct Desc
		{
			bool D3DDebug      = false;
			bool GpuValidation = false;
			f64  Timeout       = 0.0;  // seconds until RequestExit, 0 = never
			fs::path SavedDir;  // machine output: <project>/.koi (editor) or <exe>/.koi
		};

	public:
		Engine() : Engine(Desc{}) {}
		explicit Engine(const Desc& desc);
		~Engine();

		[[nodiscard]] static Engine& Get();

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

		[[nodiscard]] inline const Desc& GetDesc() const { return m_desc; }

		[[nodiscard]] inline Window&       GetWindow()       { return *m_window; }
		[[nodiscard]] inline const Window& GetWindow() const { return *m_window; }

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
		static Engine* s_instance;

		Desc                           m_desc;
		std::unique_ptr<Window>        m_window;

		// Systems below, init ordered, shutdown in reverse
		std::unique_ptr<InputManager>  m_input;
		std::unique_ptr<Renderer>      m_renderer;
		std::unique_ptr<AssetRegistry> m_assets;

		IAppHost* m_host = nullptr;  // valid for the duration of RunApp

		Timer     m_timer;
		bool      m_isRunning = true;
		PlayState m_playState = PlayState::Edit;
	};

	template<> [[nodiscard]] InputManager&  Engine::GetSubsystem<InputManager>();
	template<> [[nodiscard]] Renderer&      Engine::GetSubsystem<Renderer>();
	template<> [[nodiscard]] AssetRegistry& Engine::GetSubsystem<AssetRegistry>();
}
