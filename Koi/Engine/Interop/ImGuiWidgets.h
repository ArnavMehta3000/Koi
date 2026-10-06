#pragma once
#include "Koi/Engine/Core/Types.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <span>

namespace koi::utils
{
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
}
