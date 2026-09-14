#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Logging/LogHistory.h"
#include <imgui.h>
#include <deque>

namespace koi::editor
{
	class LogPanel : public IEditorPanel
	{
	public:
		[[nodiscard]] inline std::string_view GetName() const override { return "Log"; }
		void OnDraw() override;

	private:
		void DrawToolbar();
		void DrawCategoryFilter();
		void DrawTable();
		void DrawRow(u32 index);
		void Pull();  // drains new records out of LogHistory into the mirror
		[[nodiscard]] u32 SelectedCategories() const;

	private:
		std::deque<LogRecord>  m_records;   // mirror of the ring, trimmed to its capacity
		std::vector<u32>       m_filtered;  // indices into m_records
		std::vector<LogRecord> m_scratch;   // reused staging buffer for the drain

		std::vector<bool> m_categoryMask;   // Indexed by category id, grown as new categories intern

		ImGuiTextFilter m_textFilter;
		u64             m_cursor      = 0;
		LogLevel        m_minLevel    = LogLevel::Info;
		bool            m_filterDirty = true;
		bool            m_autoScroll  = true;
		bool            m_wrap        = false;  // wrapping costs the clipper, rows stop being uniform height
	};
}
