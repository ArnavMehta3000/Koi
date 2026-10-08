#pragma once
#include "Koi/Engine/Core/Types.h"
#include <compare>

namespace koi
{
	template <typename Tag>
	struct RGHandle
	{
		static constexpr u16 kInvalid = 0xFFFF;

		u16 Index   = kInvalid;
		u16 Version = 0;

		[[nodiscard]] constexpr bool IsValid() const noexcept { return Index != kInvalid; }
		constexpr explicit operator bool() const noexcept { return IsValid(); }

		auto operator<=>(const RGHandle&) const = default;
	};

	using RGTexture = RGHandle<struct RGTextureTag>;
}
