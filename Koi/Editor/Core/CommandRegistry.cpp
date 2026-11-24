#include "Koi/Editor/Core/CommandRegistry.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Logging/Logger.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>

namespace koi::editor
{
	namespace
	{
		constexpr const char* kPaletteId = "##CommandPalette";

		bool ContainsNoCase(std::string_view text, std::string_view needle)
		{
			return std::ranges::search(text, needle, [](char a, char b)
			{
				return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
			}).begin() != text.end() || needle.empty();
		}

		void Run(const Command& command)
		{
			KOI_LOG(Editor, Debug, "Command '{}'", command.Id);
			command.Execute();
		}
	}

	void CommandRegistry::Register(Command command)
	{
		if (Find(command.Id))
		{
			KOI_LOG(Editor, Error, "Editor command '{}' is already registered; ignoring the second one", command.Id);
			return;
		}

		KOI_LOG(Editor, Trace, "Registered editor command '{}'", command.Id);
		m_commands.push_back(std::move(command));
	}

	const Command* CommandRegistry::Find(std::string_view id) const
	{
		const auto it = std::ranges::find(m_commands, id, &Command::Id);
		return it != m_commands.end() ? &*it : nullptr;
	}

	bool CommandRegistry::Execute(std::string_view id) const
	{
		const Command* command = Find(id);
		if (!command || !CanRun(*command)) return false;

		Run(*command);
		return true;
	}

	const char* CommandRegistry::ShortcutName(const Command& command)
	{
		return command.Shortcut ? ImGui::GetKeyChordName(command.Shortcut) : "";
	}

	void CommandRegistry::PollShortcuts()
	{
		for (const Command& command : m_commands)
		{
			if (!command.Shortcut || !command.Scope.empty()) continue;

			if (ImGui::Shortcut(command.Shortcut, ImGuiInputFlags_RouteGlobal) && CanRun(command))
			{
				Run(command);
			}
		}
	}

	void CommandRegistry::PollShortcuts(std::string_view scope)
	{
		for (const Command& command : m_commands)
		{
			if (!command.Shortcut || command.Scope != scope) continue;

			if (ImGui::Shortcut(command.Shortcut) && CanRun(command))
			{
				Run(command);
			}
		}
	}

	void CommandRegistry::MenuItem(std::string_view id) const
	{
		const Command* command = Find(id);
		if (!command) return;

		const bool checked = command->IsChecked && command->IsChecked();
		if (ImGui::MenuItem(command->Label.c_str(), ShortcutName(*command), checked, CanRun(*command)))
		{
			Run(*command);
		}
	}

	void CommandRegistry::DrawPalette()
	{
		if (std::exchange(m_paletteRequested, false))
		{
			m_filter.clear();
			m_paletteIndex = 0;
			ImGui::OpenPopup(kPaletteId);
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 60.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f));
		if (!ImGui::BeginPopup(kPaletteId)) return;

		std::vector<const Command*> matches;
		for (const Command& command : m_commands)
		{
			if (CanRun(command) && ContainsNoCase(command.Label, m_filter)) matches.push_back(&command);
		}

		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (utils::SearchBar("##filter", m_filter, "Type a command...")) m_paletteIndex = 0;

		if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) ++m_paletteIndex;
		if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))   --m_paletteIndex;
		m_paletteIndex = std::clamp(m_paletteIndex, 0, std::max(static_cast<i32>(matches.size()) - 1, 0));

		const Command* chosen = nullptr;
		if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !matches.empty()) chosen = matches[static_cast<size_t>(m_paletteIndex)];

		for (size_t i = 0; i < matches.size(); ++i)
		{
			const Command& command = *matches[i];
			ImGui::PushID(command.Id.c_str());
			const std::string label = std::string(command.Icon ? command.Icon : " ") + "  " + command.Label;
			if (ImGui::Selectable(label.c_str(), static_cast<i32>(i) == m_paletteIndex)) chosen = &command;

			if (const char* shortcut = ShortcutName(command); *shortcut)
			{
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(shortcut).x + ImGui::GetCursorPosX());
				ImGui::TextDisabled("%s", shortcut);
			}
			ImGui::PopID();
		}

		if (matches.empty()) ImGui::TextDisabled("No matching command");

		if (chosen)
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();

		// After the popup is gone, so a command that opens a window of its own isn't covered by it
		if (chosen) Run(*chosen);
	}
}
