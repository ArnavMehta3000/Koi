#pragma once
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Core/Types.h"
#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <span>
#include <string>
#include <string_view>

namespace koi::utils
{
	// Bold face for headings, set by the editor once its fonts are loaded; null keeps the current font
	inline ImFont* s_headingFont = nullptr;

	inline void PushHeadingFont() { ImGui::PushFont(s_headingFont, 0.0f); }
	inline void PopHeadingFont()  { ImGui::PopFont(); }

	// One drag per component, each tagged with its axis color (X red, Y green, Z blue, W grey)
	inline bool DragAxes(const char* label, f32* values, i32 count, f32 speed = 0.05f, const char* format = "%.3f")
	{
		static constexpr ImU32 kAxisColors[4] =
		{
			IM_COL32(224, 108, 117, 255),
			IM_COL32(152, 195, 121, 255),
			IM_COL32( 97, 175, 239, 255),
			IM_COL32(140, 140, 150, 255),
		};

		bool changed = false;
		ImGui::BeginGroup();
		ImGui::PushID(label);
		ImGui::PushMultiItemsWidths(count, ImGui::CalcItemWidth());
		for (i32 i = 0; i < count; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			}
			ImGui::PushID(i);
			ImGui::SetNextItemColorMarker(kAxisColors[i % 4]);
			changed |= ImGui::DragFloat("", &values[i], speed, 0.0f, 0.0f, format);
			ImGui::PopID();
			ImGui::PopItemWidth();
		}
		ImGui::PopID();

		if (const char* labelEnd = ImGui::FindRenderedTextEnd(label); labelEnd != label)
		{
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted(label, labelEnd);
		}
		ImGui::EndGroup();
		return changed;
	}

	// Flat square button showing one icon; active draws it selected, in activeColor when one is given
	inline bool IconButton(const char* icon, const char* tooltip, bool active = false, ImVec4 activeColor = ImVec4(0, 0, 0, 0))
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const bool tinted = activeColor.w > 0.0f;

		ImVec4 background = ImVec4(0, 0, 0, 0);
		ImVec4 text       = style.Colors[ImGuiCol_Text];
		if (active)
		{
			background = tinted ? ImVec4(activeColor.x, activeColor.y, activeColor.z, 0.18f) : style.Colors[ImGuiCol_CheckboxSelectedBg];
			text       = tinted ? activeColor : style.Colors[ImGuiCol_CheckMark];
		}

		ImGui::PushStyleColor(ImGuiCol_Button, background);
		ImGui::PushStyleColor(ImGuiCol_Text, text);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		const f32 size = ImGui::GetFrameHeight();
		const bool pressed = ImGui::Button(icon, ImVec2(size, size));
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);

		if (tooltip)
		{
			ImGui::SetItemTooltip("%s", tooltip);
		}
		return pressed;
	}

	struct Segment
	{
		const char* Icon;
		const char* Tooltip;
	};

	// One-of-N icon buttons in a single framed strip; true when the selection changed
	inline bool SegmentedControl(const char* id, i32& selected, std::span<const Segment> segments)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const f32 size = ImGui::GetFrameHeight();
		const ImVec2 min = ImGui::GetCursorScreenPos();
		const ImVec2 max(min.x + size * static_cast<f32>(segments.size()), min.y + size);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);

		bool changed = false;
		ImGui::PushID(id);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, style.ItemSpacing.y));
		for (size_t i = 0; i < segments.size(); ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			ImGui::PushID(static_cast<i32>(i));
			if (IconButton(segments[i].Icon, segments[i].Tooltip, static_cast<i32>(i) == selected) && static_cast<i32>(i) != selected)
			{
				selected = static_cast<i32>(i);
				changed  = true;
			}
			ImGui::PopID();
		}
		ImGui::PopStyleVar();
		ImGui::PopID();

		if (style.FrameBorderSize > 0.0f)
		{
			drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, style.FrameBorderSize);
		}
		return changed;
	}

	[[nodiscard]] inline const char* AssetKindIcon(AssetKind kind)
	{
		switch (kind)
		{
			case AssetKind::Mesh:     return ICON_FA_SHAPES;
			case AssetKind::Texture:  return ICON_FA_IMAGE;
			case AssetKind::Shader:   return ICON_FA_CODE;
			case AssetKind::Scene:    return ICON_FA_MAP;
			case AssetKind::Material: return ICON_FA_PALETTE;
			default:                  return ICON_FA_FILE;
		}
	}

	// Text field with a search glyph and a clear button; uses the current item width
	inline bool SearchBar(const char* id, std::string& text, const char* hint = "Search...")
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const f32 width = ImGui::CalcItemWidth();
		const ImVec2 pos = ImGui::GetCursorScreenPos();
		const f32 iconWidth = ImGui::GetFontSize() + style.ItemInnerSpacing.x;

		ImGui::PushID(id);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x + iconWidth, style.FramePadding.y));
		ImGui::SetNextItemWidth(width);
		ImGui::SetNextItemAllowOverlap();
		bool changed = ImGui::InputTextWithHint("##text", hint, &text);
		ImGui::PopStyleVar();

		ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x + style.FramePadding.x, pos.y + style.FramePadding.y),
			ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_FA_MAGNIFYING_GLASS);

		if (!text.empty())
		{
			ImGui::SameLine(0.0f, 0.0f);
			ImGui::SetCursorScreenPos(ImVec2(pos.x + width - ImGui::GetFrameHeight(), pos.y));
			if (IconButton(ICON_FA_XMARK, "Clear"))
			{
				text.clear();
				changed = true;
			}
		}
		ImGui::PopID();
		return changed;
	}

	// Label-left rows: call PropertyLabel, then one widget with a "##" label, which fills the right column
	inline bool BeginPropertyGrid(const char* id)
	{
		constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
		if (!ImGui::BeginTable(id, 2, kFlags)) return false;

		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);
		return true;
	}

	inline void EndPropertyGrid() { ImGui::EndTable(); }

	inline void PropertyLabel(const char* label, const char* tooltip = nullptr)
	{
		IM_ASSERT(ImGui::GetCurrentTable() && "PropertyLabel must sit between BeginPropertyGrid and EndPropertyGrid");
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		if (tooltip)
		{
			ImGui::SetItemTooltip("%s", tooltip);
		}

		ImGui::TableNextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	// Collapsible section; with a menu, the ⋮ button or a right-click opens BeginSectionMenu() in the caller's ID scope
	inline bool SectionHeader(const char* label, bool hasMenu = false)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y + 1.0f));

		PushHeadingFont();
		const bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
		PopHeadingFont();
		if (hasMenu)
		{
			ImGui::OpenPopupOnItemClick("##SectionMenu", ImGuiPopupFlags_MouseButtonRight);

			const ImVec2 min = ImGui::GetItemRectMin();
			const ImVec2 max = ImGui::GetItemRectMax();
			ImGui::SameLine();
			ImGui::SetCursorScreenPos(ImVec2(max.x - ImGui::GetFrameHeight(), min.y));
			if (IconButton(ICON_FA_ELLIPSIS_VERTICAL, "More"))
			{
				ImGui::OpenPopup("##SectionMenu");
			}
		}

		ImGui::PopStyleVar();
		return open;
	}

	inline bool BeginSectionMenu() { return ImGui::BeginPopup("##SectionMenu"); }

	// Asset slot: icon + name (path on hover), accepts a drag from the Assets panel. True when it changed: dropped
	// holds the new virtual path, or is empty when the slot was cleared
	inline bool AssetField(const char* id, const char* icon, std::string_view name, std::string_view path, std::string& dropped, bool clearable = false)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const f32 clearWidth = clearable ? ImGui::GetFrameHeight() + style.ItemInnerSpacing.x : 0.0f;
		const std::string label = std::string(icon) + "  " + (name.empty() ? std::string("None") : std::string(name)) + "##" + id;

		bool changed = false;
		ImGui::PushID(id);
		ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_FrameBg]);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.Colors[ImGuiCol_FrameBgHovered]);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.Colors[ImGuiCol_FrameBgActive]);
		if (name.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]);
		}
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		ImGui::Button(label.c_str(), ImVec2(ImMax(ImGui::GetContentRegionAvail().x - clearWidth, 1.0f), 0.0f));
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(name.empty() ? 4 : 3);
		ImGui::SetItemTooltip("%s", path.empty() ? "Drag an asset here from the Assets panel" : std::string(path).c_str());

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kAssetDragDropType))
			{
				dropped = static_cast<const char*>(payload->Data);
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}

		if (clearable)
		{
			ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
			ImGui::BeginDisabled(name.empty());
			if (IconButton(ICON_FA_XMARK, "Clear"))
			{
				dropped.clear();
				changed = true;
			}
			ImGui::EndDisabled();
		}
		ImGui::PopID();
		return changed;
	}
}
