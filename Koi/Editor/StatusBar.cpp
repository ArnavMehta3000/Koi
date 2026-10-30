#include "Koi/Editor/StatusBar.h"
#include "Koi/Editor/ImGui/EditorPalette.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <array>
#include <format>

namespace koi::editor
{
	namespace
	{
		struct Item
		{
			std::string Text;
			ImVec4      Color;
			const char* Tooltip = nullptr;
		};
	}

	void StatusBar::Draw(const StatusInfo& info)
	{
		const EditorPalette& palette = EditorPalette::Get();
		const bool running = info.State != PlayState::Edit;

		// On the amber fill everything is dark text
		const ImVec4 text     = running ? palette.Base : palette.TextSecondary;
		const ImVec4 fill     = info.State == PlayState::Playing ? palette.Playing
		                      : info.State == PlayState::Paused  ? palette.Paused
		                      : palette.Surface;

		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, fill);
		constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
		if (ImGui::BeginViewportSideBar("##StatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(), kFlags)
			&& ImGui::BeginMenuBar())
		{
			static constexpr std::array kStateLabels = { ICON_FA_PEN "  Editing", ICON_FA_PLAY "  Playing", ICON_FA_PAUSE "  Paused" };
			ImGui::TextColored(text, "%s", kStateLabels[static_cast<size_t>(info.State)]);

			ImGui::Separator();
			const std::string_view scene = info.ScenePath.empty() ? "Untitled" : info.ScenePath.substr(info.ScenePath.find_last_of("/\\") + 1);
			ImGui::TextColored(text, "%.*s", static_cast<int>(scene.size()), scene.data());
			if (!info.ScenePath.empty())
			{
				ImGui::SetItemTooltip("%.*s", static_cast<int>(info.ScenePath.size()), info.ScenePath.data());
			}

			// Right-aligned group, measured first so it hugs the edge
			std::array<Item, 2> right;
			size_t count = 0;
			if (info.Importing)
			{
				right[count++] = { ICON_FA_SPINNER "  Importing", text };
			}
			right[count++] = { std::format("{:.0f} fps", ImGui::GetIO().Framerate), text };

			const f32 spacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
			f32 width = 0.0f;
			for (size_t i = 0; i < count; ++i)
			{
				width += ImGui::CalcTextSize(right[i].Text.c_str()).x + spacing;
			}

			ImGui::SameLine(ImMax(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - width));
			for (size_t i = 0; i < count; ++i)
			{
				if (i > 0)
				{
					ImGui::SameLine(0.0f, spacing);
				}
				ImGui::TextColored(right[i].Color, "%s", right[i].Text.c_str());
				if (right[i].Tooltip)
				{
					ImGui::SetItemTooltip("%s", right[i].Tooltip);
				}
			}

			ImGui::EndMenuBar();
		}
		ImGui::End();
		ImGui::PopStyleColor();
	}
}
