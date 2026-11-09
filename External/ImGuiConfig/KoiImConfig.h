#pragma once

// IMGUI_USER_CONFIG, set in Xmake/Packages/ImGui.lua
#include <source_location>
#include <string_view>

namespace koi::internal
{
	void OnAssertFailed(std::string_view condition, std::string_view message, const std::source_location& where);
}

// Mirrors KOI_ASSERT (Koi/Engine/Core/Assert.h) without pulling in engine headers
#ifndef NDEBUG
	#define IM_ASSERT(_EXPR) do { if (!(_EXPR)) [[unlikely]] { ::koi::internal::OnAssertFailed(#_EXPR, {}, ::std::source_location::current()); } } while (0)
#endif

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS

#ifndef IMGUI_DEFINE_MATH_OPERATORS
	#define IMGUI_DEFINE_MATH_OPERATORS
#endif
