#pragma once
#include "Koi/Engine/Core/Types.h"
#include <imgui.h>
#include <imgui_internal.h>

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
}
