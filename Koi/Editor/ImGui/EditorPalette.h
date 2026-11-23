#pragma once
#include <imgui.h>

namespace koi::editor
{
	// Named colours a theme provides; panels read these instead of hard-coding
	struct EditorPalette
	{
		ImVec4 Base;           // darkest background
		ImVec4 Surface;        // one step up: title bars, selected tabs
		ImVec4 Elevated;       // popups, table headers
		ImVec4 Raised;         // frames, buttons, headers
		ImVec4 RaisedHover;
		ImVec4 RaisedActive;
		ImVec4 Border;
		ImVec4 BorderStrong;
		ImVec4 Selection;
		ImVec4 Text;
		ImVec4 TextSecondary;
		ImVec4 TextDisabled;
		ImVec4 Accent;
		ImVec4 AccentHover;
		ImVec4 AccentMuted;
		ImVec4 Link;
		ImVec4 Info;
		ImVec4 Success;
		ImVec4 Warning;
		ImVec4 Error;
		ImVec4 Playing;        // amber, tints the editor while the game runs
		ImVec4 Paused;

		// The active theme's palette, set by utils::ApplyTheme
		[[nodiscard]] static const EditorPalette& Get();
		static void SetActive(const EditorPalette& palette);

	private:
		static const EditorPalette* s_active;
	};
}
