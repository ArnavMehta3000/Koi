#include "Koi/Editor/Panels/LogPanel.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <algorithm>
#include <cstdio>

namespace koi
{
	namespace
	{
		ImVec4 LevelColor(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Trace:    return { 0.55f, 0.55f, 0.55f, 1.0f };
			case LogLevel::Debug:    return { 0.45f, 0.70f, 0.95f, 1.0f };
			case LogLevel::Info:     return { 0.85f, 0.85f, 0.85f, 1.0f };
			case LogLevel::Warn:     return { 0.95f, 0.75f, 0.25f, 1.0f };
			case LogLevel::Error:    return { 0.95f, 0.35f, 0.30f, 1.0f };
			case LogLevel::Critical: return { 1.00f, 0.25f, 0.65f, 1.0f };
			default:                 return { 1.00f, 1.00f, 1.00f, 1.0f };
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
	}

	u32 LogPanel::SelectedCategories() const
	{
		// Derived rather than tracked - a handful of entries, and it can't drift
		return static_cast<u32>(std::ranges::count(m_categoryMask, true));
	}

	void LogPanel::Pull()
	{
		m_scratch.clear();
		m_cursor = LogHistory::Get().ReadFrom(m_cursor, m_scratch);
		if (m_scratch.empty())
			return;

		for (LogRecord& record : m_scratch)
			m_records.push_back(std::move(record));

		const size_t capacity = LogHistory::Get().Capacity();
		while (capacity > 0 && m_records.size() > capacity)
			m_records.pop_front();

		m_filterDirty = true;
	}

	void LogPanel::OnDraw()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Editor);

		if (ImGui::Begin("Log"))
		{
			Pull();
			DrawToolbar();
			ImGui::Separator();
			DrawTable();
		}
		ImGui::End();
	}

	void LogPanel::DrawToolbar()
	{
		if (ImGui::Button("Clear"))
		{
			LogHistory::Get().Clear();
			m_records.clear();
			m_filtered.clear();
			m_cursor      = 0;
			m_filterDirty = true;
		}

		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &m_autoScroll);

		ImGui::SameLine();
		ImGui::Checkbox("Wrap", &m_wrap);
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Wraps long messages. Rows stop being uniform height, so the\nlist clipper is disabled and every filtered row is laid out.");

		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		if (ImGui::BeginCombo("##level", ToString(m_minLevel).data()))
		{
			for (i32 i = 0; i < LogLevelCount; ++i)
			{
				const auto level      = static_cast<LogLevel>(i);
				const bool isSelected = (level == m_minLevel);
				ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(level));
				if (ImGui::Selectable(ToString(level).data(), isSelected))
				{
					m_minLevel    = level;
					m_filterDirty = true;
				}
				ImGui::PopStyleColor();
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		DrawCategoryFilter();

		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1.0f);
		if (m_textFilter.Draw("##search"))
			m_filterDirty = true;
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
			std::snprintf(preview, sizeof(preview), "All sources");
		else if (selected == 1)
			std::snprintf(preview, sizeof(preview), "%s", CategoryName(FirstSelected(m_categoryMask)).data());
		else
			std::snprintf(preview, sizeof(preview), "%u sources", selected);

		ImGui::SetNextItemWidth(180.0f);
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
		ImGui::Separator();

		for (u16 i = 0; i < count; ++i)
		{
			// Copied out rather than bound: vector<bool> hands back a proxy, not a bool&
			bool enabled = m_categoryMask[i];

			ImGui::PushID(i);
			if (ImGui::Checkbox(CategoryName(i).data(), &enabled))
			{
				m_categoryMask[i] = enabled;
				m_filterDirty     = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", Logger::GetCategory(i).Path.data());
			ImGui::PopID();
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

				if ((record.Level < m_minLevel)
					|| (byCategory && (record.Category >= m_categoryMask.size() || !m_categoryMask[record.Category]))
					|| (!m_textFilter.PassFilter(record.Message.c_str())))
				{
					continue;
				}

				m_filtered.push_back(i);
			}
			m_filterDirty = false;
		}

		ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg
		                      | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV;

		// ScrollX would force the message column to a fixed width, which is exactly
		// what wrapping needs not to happen
		if (!m_wrap)
			flags |= ImGuiTableFlags_ScrollX;

		if (!ImGui::BeginTable("##log", 4, flags))
			return;

		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Time",   ImGuiTableColumnFlags_WidthFixed, 64.0f);
		ImGui::TableSetupColumn("Level",  ImGuiTableColumnFlags_WidthFixed, 64.0f);
		ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Message", m_wrap
			? ImGuiTableColumnFlags_WidthStretch
			: ImGuiTableColumnFlags_WidthFixed, m_wrap ? 0.0f : 1400.0f);
		ImGui::TableHeadersRow();

		const auto rowCount = static_cast<i32>(m_filtered.size());

		if (m_wrap)
		{
			// Variable row heights, so no clipper. ImGui still culls the draw for
			// offscreen rows; what we pay is text layout on every filtered row
			for (i32 row = 0; row < rowCount; ++row)
				DrawRow(static_cast<u32>(row));
		}
		else
		{
			ImGuiListClipper clipper;
			clipper.Begin(rowCount);
			while (clipper.Step())
			{
				for (i32 row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
				{
					DrawRow(static_cast<u32>(row));
				}
			}
		}

		if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);

		ImGui::EndTable();
	}

	void LogPanel::DrawRow(u32 row)
	{
		const LogRecord& record = m_records[m_filtered[row]];
		const ImVec4     color  = LevelColor(record.Level);

		ImGui::TableNextRow();
		ImGui::PushID(static_cast<i32>(row));

		ImGui::TableSetColumnIndex(0);
		ImGui::TextDisabled("%8.3f", record.Time);

		ImGui::TableSetColumnIndex(1);
		ImGui::TextColored(color, "%s", ToString(record.Level).data());

		ImGui::TableSetColumnIndex(2);
		const LogCategoryInfo category = Logger::GetCategory(record.Category);
		ImGui::Text("%s:%u", CategoryName(record.Category).data(), record.Line);
		if (ImGui::IsItemHovered() && !category.Path.empty())
			ImGui::SetTooltip("%s:%u", category.Path.data(), record.Line);

		ImGui::TableSetColumnIndex(3);
		if (m_wrap)
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);

		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(record.Message.c_str());  // Unformatted since text can contain '%'
		ImGui::PopStyleColor();

		if (m_wrap) ImGui::PopTextWrapPos();

		if (ImGui::BeginPopupContextItem("##ctx"))
		{
			if (ImGui::MenuItem("Copy message"))
				ImGui::SetClipboardText(record.Message.c_str());
			if (ImGui::MenuItem("Copy source"))
			{
				const std::string source = std::string(category.Path) + ":" + std::to_string(record.Line);
				ImGui::SetClipboardText(source.c_str());
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();
	}
}
