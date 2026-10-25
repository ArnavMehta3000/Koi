#pragma once
#include "Koi/Engine/Core/Types.h"
#include <imgui.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace koi::editor
{
	struct Command
	{
		std::string           Id;               // "play.toggle"
		std::string           Label;            // menu and palette text
		const char*           Icon     = nullptr;
		ImGuiKeyChord         Shortcut = 0;
		std::string           Scope;            // empty = global; otherwise the panel that polls it while focused
		std::function<void()> Execute;
		std::function<bool()> CanExecute;       // empty = always
		std::function<bool()> IsChecked;        // empty = not a toggle
	};

	// Every editor action in one place, so menus, buttons, shortcuts and the palette agree
	class CommandRegistry : public NonCopyable
	{
	public:
		void Register(Command command);

		[[nodiscard]] const Command* Find(std::string_view id) const;
		bool Execute(std::string_view id) const;

		// "Ctrl+P", or empty. ImGui reuses the buffer on the next call
		[[nodiscard]] static const char* ShortcutName(const Command& command);

		// Global shortcuts, once per frame after the panels
		void PollShortcuts();

		// A scope's shortcuts, routed to the focused window; call from inside that panel's window
		void PollShortcuts(std::string_view scope);

		// A menu item for the command: label, shortcut hint, check mark, disabled when it can't run
		void MenuItem(std::string_view id) const;

		void OpenPalette() { m_paletteRequested = true; }
		void DrawPalette();

	private:
		[[nodiscard]] static bool CanRun(const Command& command) { return command.Execute && (!command.CanExecute || command.CanExecute()); }

	private:
		std::vector<Command> m_commands;
		std::string          m_filter;
		i32                  m_paletteIndex     = 0;
		bool                 m_paletteRequested = false;
	};
}
