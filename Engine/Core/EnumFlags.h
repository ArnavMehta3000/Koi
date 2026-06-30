#pragma once
// Companion macro header for Koi.Core.EnumFlags.
// Import Koi.Core.EnumFlags before using these macros.

#define KOI_ENABLE_ENUM_FLAGS(EnumType) \
	template <> struct ::koi::EnableEnumFlags<EnumType> : std::true_type {}

#define KOI_FLAG_ENTRY(Name) ::koi::EnumFlagEntry{ Name, #Name },

#define KOI_ENUM_FLAG_NAMES(EnumType, ...)                        \
	template <>                                                   \
	struct ::koi::EnumFlagNameTable<EnumType>                     \
	{                                                             \
		[[nodiscard]] static constexpr auto Entries() noexcept    \
		{                                                         \
			using enum EnumType;                                  \
			return std::array{ __VA_ARGS__ };                     \
		}                                                         \
	}
