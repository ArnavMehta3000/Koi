#pragma once
#include "Koi/Engine/Core/Types.h"
#include <array>
#include <imgui.h>
#include <string_view>

namespace koi
{
	// Combo over an enum whose values index names
	template<typename E, size_t N>
	bool DrawEnumCombo(const char* label, E& value, const std::array<std::string_view, N>& names)
	{
		int index = static_cast<int>(value);
		const bool changed = ImGui::Combo(
			label,
			&index,
			[](void* data, int i)
			{
				return static_cast<const std::array<std::string_view, N>*>(data)->at(static_cast<size_t>(i)).data();
			},
			const_cast<std::array<std::string_view, N>*>(&names),  // data
			static_cast<int>(N));  //size

		if (changed)
		{
			value = static_cast<E>(index);
		}

		return changed;
	}
}
