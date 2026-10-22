#include "Koi/Editor/Panels/LogPanel.h"
#include "Koi/Editor/Core/EditorContext.h"
#include "Koi/Editor/ImGui/EditorPalette.h"
#include "Koi/Editor/PlaySession.h"
#include "Koi/Editor/UI/Widgets.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace koi::editor
{
	namespace
	{
		struct GroupInfo
		{
			const char* Icon;
			const char* Tooltip;
		};

		constexpr std::array<GroupInfo, 4> kGroups =
		{{
			{ ICON_FA_BUG,                  "Verbose (Trace, Debug)" },
			{ ICON_FA_CIRCLE_INFO,          "Info" },
			{ ICON_FA_TRIANGLE_EXCLAMATION, "Warnings" },
			{ ICON_FA_CIRCLE_XMARK,         "Errors" },
		}};

		u32 LevelGroup(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Trace:
			case LogLevel::Debug: return 0;
			case LogLevel::Info:  return 1;
			case LogLevel::Warn:  return 2;
			default:              return 3;
			}
		}

		ImVec4 LevelColor(LogLevel level)
		{
			const EditorPalette& palette = EditorPalette::Get();
			switch (level)
			{
			case LogLevel::Trace:    return palette.TextDisabled;
			case LogLevel::Debug:    return palette.Info;
			case LogLevel::Warn:     return palette.Warning;
			case LogLevel::Error:
			case LogLevel::Critical: return palette.Error;
			default:                 return palette.Text;
			}
		}

		ImVec4 GroupColor(u32 group)
		{
			const EditorPalette& palette = EditorPalette::Get();
			switch (group)
			{
			case 0:  return palette.Info;
			case 2:  return palette.Warning;
			case 3:  return palette.Error;
			default: return palette.Text;
			}
		}

		const char* LevelIcon(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Trace:    return ICON_FA_SHOE_PRINTS;
			case LogLevel::Debug:    return ICON_FA_BUG;
			case LogLevel::Info:     return ICON_FA_CIRCLE_INFO;
			case LogLevel::Warn:     return ICON_FA_TRIANGLE_EXCLAMATION;
			case LogLevel::Error:    return ICON_FA_CIRCLE_XMARK;
			default:                 return ICON_FA_SKULL;
			}
		}

		std::string_view CategoryName(u16 id)
		{
			const std::string_view name = Logger::GetCategory(id).Name;
			return name.empty() ? std::string_view("<unknown>") : name;
		}

		u16 FirstSelected(const std::vector<bool>& mask)
		{
			return static_cast<u16>(std::ranges::find(mask, true) - mask.begin());
		}

		void FormatChip(char (&label)[32], u32 group, u32 count)
		{
			std::snprintf(label, sizeof(label), "%s %u###chip%u", kGroups[group].Icon, count, group);
		}

		f32 ChipWidth(u32 group, u32 count)
		{
			char label[32];
			FormatChip(label, group, count);
			return ImGui::CalcTextSize(label, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
		}

		// Count + icon toggle, tinted with its level colour while shown
		void CounterChip(u32 group, u32 count, bool& shown)
		{
			const EditorPalette& palette = EditorPalette::Get();
			const ImVec4 color = GroupColor(group);

			char label[32];
			FormatChip(label, group, count);

			ImGui::PushStyleColor(ImGuiCol_Button, shown ? ImVec4(color.x, color.y, color.z, 0.16f) : ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Text, shown ? color : palette.TextDisabled);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			if (ImGui::Button(label))
			{
				shown = !shown;
			}
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(2);
			ImGui::SetItemTooltip("%s: %s", kGroups[group].Tooltip, shown ? "shown" : "hidden");
		}

		// Game categories stand out against the engine's own
		void CategoryPill(u16 category)
		{
			const EditorPalette& palette = EditorPalette::Get();
			const bool game = Logger::GetCategory(category).Origin == LogOrigin::Game;
			const std::string_view name = CategoryName(category);
			const char* begin = name.data();
			const char* end   = begin + name.size();

			const f32    pad  = ImGui::GetStyle().FramePadding.x * 0.75f;
			const ImVec2 pos  = ImGui::GetCursorScreenPos();
			const ImVec2 size = ImGui::CalcTextSize(begin, end);
			const f32    top  = pos.y + ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset;
			ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(pos.x, top), ImVec2(pos.x + size.x + pad * 2.0f, top + size.y),
				ImGui::GetColorU32(game ? palette.AccentMuted : palette.Raised), size.y * 0.5f);

			ImGui::SetCursorScreenPos(ImVec2(pos.x + pad, pos.y));
			ImGui::PushStyleColor(ImGuiCol_Text, game ? palette.Text : palette.TextSecondary);
			ImGui::TextUnformatted(begin, end);
			ImGui::PopStyleColor();
		}
	}

	LogPanel::LogPanel(EditorContext* ctx)
	{
		ctx->Get<PlaySession>()->OnStarted([this] { OnPlay(); });
	}

	void LogPanel::OnPlay()
	{
		if (m_clearOnPlay)
		{
			Clear();
		}
	}

	u32 LogPanel::SelectedCategories() const
	{
		return static_cast<u32>(std::ranges::count(m_categoryMask, true));
	}

	void LogPanel::Pull()
	{
		m_scratch.clear();
		m_cursor = LogHistory::Get().ReadFrom(m_cursor, m_scratch);
		if (m_scratch.empty())
			return;

		for (LogRecord& record : m_scratch)
		{
			++m_counts[LevelGroup(record.Level)];
			m_records.push_back(std::move(record));
		}

		const size_t capacity = LogHistory::Get().Capacity();
		while (capacity > 0 && m_records.size() > capacity)
		{
			--m_counts[LevelGroup(m_records.front().Level)];
			m_records.pop_front();

			if (m_selected != kNoSelection)
			{
				if (m_selected == 0)
					Select(kNoSelection);
				else
					--m_selected;
			}
		}

		m_filterDirty = true;
	}

	void LogPanel::Clear()
	{
		LogHistory::Get().Clear();
		m_records.clear();
		m_filtered.clear();
		m_counts      = {};
		m_cursor      = 0;
		m_filterDirty = true;
		Select(kNoSelection);
	}

	void LogPanel::Select(u32 record)
	{
		m_selected = record;
		if (record == kNoSelection)
			m_detailText.clear();
		else
			m_detailText = m_records[record].Message;
	}

	void LogPanel::OnDraw()
	{
		Pull();
		DrawToolbar();
		DrawTable();
		DrawDetail();
	}

	void LogPanel::DrawToolbar()
	{
		if (utils::IconButton(ICON_FA_TRASH_CAN, "Clear"))
		{
			Clear();
		}

		ImGui::SameLine();
		if (utils::IconButton(ICON_FA_BROOM, "Clear on Play", m_clearOnPlay))
		{
			m_clearOnPlay = !m_clearOnPlay;
		}

		ImGui::SameLine();
		DrawCategoryFilter();

		const ImGuiStyle& style = ImGui::GetStyle();
		f32 chipsWidth = 0.0f;
		for (u32 group = 0; group < kGroupCount; ++group)
		{
			chipsWidth += ChipWidth(group, m_counts[group]) + style.ItemInnerSpacing.x;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImMax(ImGui::GetContentRegionAvail().x - chipsWidth - style.ItemSpacing.x, 80.0f));
		if (utils::SearchBar("##search", m_search, "Filter messages..."))
		{
			ImStrncpy(m_textFilter.InputBuf, m_search.c_str(), IM_COUNTOF(m_textFilter.InputBuf));
			m_textFilter.Build();
			m_filterDirty = true;
		}

		for (u32 group = 0; group < kGroupCount; ++group)
		{
			ImGui::SameLine(0.0f, group == 0 ? style.ItemSpacing.x : style.ItemInnerSpacing.x);
			const bool wasShown = m_showGroup[group];
			CounterChip(group, m_counts[group], m_showGroup[group]);
			m_filterDirty |= wasShown != m_showGroup[group];
		}
	}

	// Built from the intern table, not from the records, so a category doesn't
	// disappear from the list once its last message ages out of the ring
	void LogPanel::DrawCategoryFilter()
	{
		const u16 count = Logger::CategoryCount();
		if (m_categoryMask.size() < count)
			m_categoryMask.resize(count, false);  // new categories start unselected, i.e. unfiltered

		const u32 selected = SelectedCategories();

		char preview[64];
		if (selected == 0)
			std::snprintf(preview, sizeof(preview), ICON_FA_FILTER "  All categories");
		else if (selected == 1)
			std::snprintf(preview, sizeof(preview), ICON_FA_FILTER "  %s", CategoryName(FirstSelected(m_categoryMask)).data());
		else
			std::snprintf(preview, sizeof(preview), ICON_FA_FILTER "  %u categories", selected);

		ImGui::SetNextItemWidth(170.0f);
		if (!ImGui::BeginCombo("##category", preview, ImGuiComboFlags_HeightLarge))
			return;

		if (ImGui::SmallButton("Show all"))
		{
			m_categoryMask.assign(m_categoryMask.size(), false);
			m_filterDirty = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Invert"))
		{
			m_categoryMask.flip();
			m_filterDirty = true;
		}

		static constexpr std::array<std::pair<LogOrigin, const char*>, 3> kOrigins =
		{{
			{ LogOrigin::Game,   "Game" },
			{ LogOrigin::Engine, "Engine" },
			{ LogOrigin::Editor, "Editor" },
		}};

		for (const auto& [origin, label] : kOrigins)
		{
			ImGui::SeparatorText(label);
			for (u16 i = 0; i < count; ++i)
			{
				if (Logger::GetCategory(i).Origin != origin)
					continue;

				// Copied out rather than bound: vector<bool> hands back a proxy, not a bool&
				bool enabled = m_categoryMask[i];

				ImGui::PushID(i);
				if (ImGui::Checkbox(CategoryName(i).data(), &enabled))
				{
					m_categoryMask[i] = enabled;
					m_filterDirty     = true;
				}
				ImGui::SetItemTooltip("Logs %s and above", koi::utils::ToString(Logger::GetCategoryLevel(i)).data());
				ImGui::PopID();
			}
		}

		ImGui::EndCombo();
	}

	void LogPanel::DrawTable()
	{
		if (m_filterDirty)
		{
			const bool byCategory = SelectedCategories() > 0;

			m_filtered.clear();
			for (u32 i = 0; i < static_cast<u32>(m_records.size()); ++i)
			{
				const LogRecord& record = m_records[i];

				if (!m_showGroup[LevelGroup(record.Level)]
					|| (byCategory && (record.Category >= m_categoryMask.size() || !m_categoryMask[record.Category]))
					|| (!m_textFilter.PassFilter(record.Message.c_str())))
				{
					continue;
				}

				m_filtered.push_back(i);
			}
			m_filterDirty = false;
		}

		const ImGuiStyle& style = ImGui::GetStyle();
		const f32 detailHeight = (m_selected == kNoSelection) ? 0.0f
			: ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 4.0f + style.FramePadding.y * 2.0f + style.ItemSpacing.y;

		constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable
		                                 | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuterH;
		if (!ImGui::BeginTable("##log", 4, kFlags, ImVec2(0.0f, -detailHeight)))
			return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("##level", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ImGui::GetFontSize() * 1.25f);
		ImGui::TableSetupColumn("Time",    ImGuiTableColumnFlags_WidthFixed, 64.0f);
		ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<i32>(m_filtered.size()));
		while (clipper.Step())
		{
			for (i32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				DrawRow(static_cast<u32>(row));
			}
		}

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndTable();
	}

	void LogPanel::DrawRow(u32 row)
	{
		const u32        index  = m_filtered[row];
		const LogRecord& record = m_records[index];
		const ImVec4     color  = LevelColor(record.Level);

		ImGui::TableNextRow();
		ImGui::PushID(static_cast<i32>(row));

		ImGui::TableSetColumnIndex(0);
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		if (ImGui::Selectable(LevelIcon(record.Level), index == m_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
		{
			Select(index == m_selected ? kNoSelection : index);
		}
		ImGui::PopStyleColor();

		const LogSourceInfo source = Logger::GetSource(record.Source);
		if (ImGui::BeginPopupContextItem("##ctx"))
		{
			if (ImGui::MenuItem(ICON_FA_COPY "  Copy message"))
				ImGui::SetClipboardText(record.Message.c_str());
			if (ImGui::MenuItem(ICON_FA_FILE_CODE "  Copy source"))
			{
				const std::string location = std::string(source.Path) + ":" + std::to_string(record.Line);
				ImGui::SetClipboardText(location.c_str());
			}
			ImGui::EndPopup();
		}

		ImGui::TableSetColumnIndex(1);
		ImGui::TextDisabled("%8.3f", record.Time);

		ImGui::TableSetColumnIndex(2);
		CategoryPill(record.Category);
		if (!source.Path.empty())
			ImGui::SetItemTooltip("%s:%u", source.Path.data(), record.Line);

		// First line only, rows stay one line high for the clipper; the detail strip shows the rest
		ImGui::TableSetColumnIndex(3);
		const char* begin   = record.Message.c_str();
		const char* newline = std::strchr(begin, '\n');
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(begin, newline ? newline : begin + record.Message.size());
		ImGui::PopStyleColor();
		if (newline)
		{
			ImGui::SameLine();
			ImGui::TextDisabled(ICON_FA_ELLIPSIS);
		}

		ImGui::PopID();
	}

	void LogPanel::DrawDetail()
	{
		if (m_selected == kNoSelection)
			return;

		const LogRecord&      record   = m_records[m_selected];
		const LogSourceInfo  source  = Logger::GetSource(record.Source);
		const EditorPalette& palette = EditorPalette::Get();

		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(LevelColor(record.Level), "%s  %s", LevelIcon(record.Level), koi::utils::ToString(record.Level).data());
		ImGui::SameLine();
		ImGui::TextDisabled("%.3fs", record.Time);
		ImGui::SameLine();
		CategoryPill(record.Category);
		ImGui::SameLine();
		ImGui::TextColored(palette.TextSecondary, "%s:%u", source.Path.data(), record.Line);

		const f32 buttons = ImGui::GetFrameHeight() * 3.0f + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImMax(ImGui::GetContentRegionAvail().x - buttons, 0.0f));
		if (utils::IconButton(ICON_FA_COPY, "Copy message"))
		{
			ImGui::SetClipboardText(record.Message.c_str());
		}
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		if (utils::IconButton(ICON_FA_FILE_CODE, "Copy source"))
		{
			const std::string location = std::string(source.Path) + ":" + std::to_string(record.Line);
			ImGui::SetClipboardText(location.c_str());
		}
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		if (utils::IconButton(ICON_FA_XMARK, "Close"))
		{
			Select(kNoSelection);
			return;
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, palette.Base);
		ImGui::InputTextMultiline("##message", &m_detailText, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_WordWrap);
		ImGui::PopStyleColor();
	}
}
