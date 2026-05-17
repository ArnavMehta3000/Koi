#pragma once
#include "Core/Types.h"

namespace koi
{
	enum class MemTag : u8
	{
		Untagged = 0,
		Engine,
		Renderer,
		Assets,
		Scripting,
		Temp,

		MAX_COUNT
	};

	constexpr std::string_view ToString(MemTag tag)
	{
#define X(tag) case tag: return #tag;
		using enum MemTag;

		switch (tag)
		{
			X(Untagged)
			X(Engine)
			X(Renderer)
			X(Assets)
			X(Scripting)
			X(Temp)
		default: return "<?>";
		}
#undef X
	}
}
