#include "Koi/Editor/StatusBar.h"
#include "Koi/Editor/ImGui/EditorPalette.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <array>
#include <filesystem>
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

	void StatusBar::PullLog()
	{
		m_scratch.clear();
		m_logCursor = LogHistory::Get().ReadFrom(m_logCursor, m_scratch);
		for (LogRecord& record : m_scratch)
		{
			if (record.Level == LogLevel::Warn)
			{
				++m_warnings;
				m_lastWarning = std::move(record.Message);
			}
			else if (record.Level == LogLevel::Error || record.Level == LogLevel::Critical)
			{
				++m_errors;
				m_lastError = std::move(record.Message);
			}
		}
	}

	void StatusBar::Draw(const StatusInfo& info)
	{
		PullLog();

		const EditorPalette& palette = EditorPalette::Get();
		const bool running = info.State != PlayState::Edit;

		// On the amber fill everything is dark text; otherwise counts take their meaning colour
		const ImVec4 text     = running ? palette.Base : palette.TextSecondary;
		const ImVec4 warning  = running ? palette.Base : (m_warnings > 0 ? palette.Warning : palette.TextDisabled);
		const ImVec4 error    = running ? palette.Base : (m_errors > 0 ? palette.Error : palette.TextDisabled);
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
			const std::string scene = info.ScenePath.empty() ? "Untitled" : std::filesystem::path(info.ScenePath).filename().string();
			ImGui::TextColored(text, "%s", scene.c_str());
			if (!info.ScenePath.empty())
			{
				ImGui::SetItemTooltip("%.*s", static_cast<int>(info.ScenePath.size()), info.ScenePath.data());
			}

			// Right-aligned group, measured first so it hugs the edge
			std::vector<Item> right;
			if (info.Importing)
			{
				right.push_back({ ICON_FA_SPINNER "  Importing", text });
			}
			right.push_back({ std::format(ICON_FA_TRIANGLE_EXCLAMATION " {}", m_warnings), warning, m_lastWarning.empty() ? nullptr : m_lastWarning.c_str() });
			right.push_back({ std::format(ICON_FA_CIRCLE_XMARK " {}", m_errors), error, m_lastError.empty() ? nullptr : m_lastError.c_str() });
			right.push_back({ std::format("{:.0f} fps", ImGui::GetIO().Framerate), text });

			const f32 spacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
			f32 width = 0.0f;
			for (const Item& item : right)
			{
				width += ImGui::CalcTextSize(item.Text.c_str()).x + spacing;
			}

			ImGui::SameLine(ImMax(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - width));
			for (size_t i = 0; i < right.size(); ++i)
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
