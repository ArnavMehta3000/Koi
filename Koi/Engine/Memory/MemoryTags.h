#pragma once
#include "Koi/Engine/Core/Types.h"

namespace koi
{
	enum class MemTag : u8
	{
		Untagged = 0,
		Engine,
		Editor,
		Logging,
		Platform,
		Input,
		Renderer,
		RHI,
		Assets,
		Scripting,
		User,
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
			X(Editor)
			X(Logging)
			X(Platform)
			X(Input)
			X(Renderer)
			X(RHI)
			X(Assets)
			X(Scripting)
			X(User)
			X(Temp)
		default: return "<?>";
		}
#undef X
	}
}
