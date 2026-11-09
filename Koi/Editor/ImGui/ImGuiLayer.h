#pragma once
#include "Koi/Engine/Core/Types.h"

namespace koi
{
	class CommandList;
	class Window;
	enum class PlayState : u8;
}

namespace koi::editor
{
	enum class Theme : u8;

	class ImGuiLayer : public NonCopyable, public NonMovable
	{
	public:
		~ImGuiLayer();

		void Init(Window& window);
		void Shutdown();

		void BeginFrame(bool gameOwnsKeyboard);
		void EndFrame(CommandList& cmd);

		// Contributed to whichever menu the editor puts it in
		void DrawAppearanceMenu();

		// Tints the theme while the game runs; cheap to call every frame
		void SetPlayState(PlayState state);

	private:
		void RegisterSettingsHandler();
		void ApplyAppearance();
		void SetFontScale(f32 scale);

	private:
		Theme m_theme{};  // real default is applied in Init, where Themes.h is visible
		f32   m_dpiScale  = 1.0f;
		f32   m_fontScale = 1.0f;  // on top of the DPI scale, saved in imgui.ini
		f32   m_fontScaleEdit = 1.0f;  // slider value, applied on release so the menu doesn't resize under the mouse
		PlayState m_playState{};
		Window* m_window    = nullptr;
		u32   m_hookId      = 0;
		bool  m_initialized = false;
		bool  m_loadDefaultLayout = false;  // on the first frame, once every settings handler is registered
	};
}
