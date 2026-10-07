#pragma once
#include "Koi/Engine/Interop/EditorPanel.h"
#include "Koi/Engine/Logging/LogHistory.h"
#include <imgui.h>
#include <array>
#include <deque>
#include <string>
#include <IconsFontAwesome6.h>

namespace koi::editor
{
	class LogPanel : public IEditorPanel
	{
	public:
		[[nodiscard]] inline std::string_view GetName() const override { return "Log"; }
		[[nodiscard]] const char* GetIcon() const override { return ICON_FA_TERMINAL; }
		void OnDraw() override;

		// Called by the editor as Play starts, before the game logs anything
		void OnPlay();

	private:
		// Verbose (Trace + Debug), Info, Warn, Error (+ Critical)
		static constexpr u32 kGroupCount = 4;
		static constexpr u32 kNoSelection = ~0u;

		void Pull();  // drains new records out of LogHistory into the mirror
		void Clear();
		void Select(u32 record);
		void DrawToolbar();
		void DrawCategoryFilter();
		void DrawTable();
		void DrawRow(u32 row);
		void DrawDetail();
		[[nodiscard]] u32 SelectedCategories() const;

	private:
		std::deque<LogRecord>  m_records;   // mirror of the ring, trimmed to its capacity
		std::vector<u32>       m_filtered;  // indices into m_records
		std::vector<LogRecord> m_scratch;   // reused staging buffer for the drain

		std::vector<bool> m_categoryMask;   // Indexed by category id, grown as new categories intern

		std::array<u32, kGroupCount>  m_counts{};
		std::array<bool, kGroupCount> m_showGroup = { false, true, true, true };

		ImGuiTextFilter m_textFilter;
		std::string     m_search;      // SearchBar text, copied into m_textFilter
		std::string     m_detailText;  // selected message, owned so the read-only InputText can select from it
		u64             m_cursor      = 0;
		u32             m_selected    = kNoSelection;  // index into m_records
		bool            m_filterDirty = true;
		bool            m_clearOnPlay = false;
	};
}
