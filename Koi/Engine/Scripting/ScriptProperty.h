#pragma once
#include "Koi/Engine/Core/Types.h"
#include <string>
#include <variant>

namespace koi
{
	enum class ScriptPropertyType : u8
	{
		Bool,
		Int,
		Float,
		String,
	};

	struct ScriptProperty
	{
		std::string        Name;
		ScriptPropertyType Type;
		std::variant<bool, i32, f32, std::string> Value;
	};
}
