#pragma once
#include "Koi/Engine/Core/Types.h"
#include <type_traits>
#include <concepts>
#include <format>
#include <string>

namespace koi
{
	template <typename E>
	struct EnableEnumFlags : std::false_type {};

#define KOI_ENABLE_ENUM_FLAGS(EnumType) template <> struct ::koi::EnableEnumFlags<EnumType> : std::true_type {}

	template <typename E>
	concept EnumFlagType = std::is_enum_v<E> && EnableEnumFlags<E>::value;

	template <typename E>
	class EnumFlag
	{
	public:
		using EnumType = E;
		using UnderlyingType = std::underlying_type_t<E>;

		constexpr EnumFlag() noexcept = default;
		constexpr EnumFlag(E value) noexcept : m_mask(ToUnderlying(value)) {}
		constexpr explicit EnumFlag(UnderlyingType raw) noexcept : m_mask(raw) {}

		[[nodiscard]] constexpr bool Has(E flag)          const noexcept { return (m_mask & ToUnderlying(flag)) == ToUnderlying(flag); }
		[[nodiscard]] constexpr bool HasAny(EnumFlag f)   const noexcept { return (m_mask & f.m_mask) != 0;                            }
		[[nodiscard]] constexpr bool HasAll(EnumFlag f)   const noexcept { return (m_mask & f.m_mask) == f.m_mask;                     }
		[[nodiscard]] constexpr bool IsEmpty()            const noexcept { return m_mask == 0;                                         }
		[[nodiscard]] constexpr explicit operator bool()  const noexcept { return m_mask != 0;                                         }

		constexpr EnumFlag& Set(E flag)       noexcept { m_mask |= ToUnderlying(flag);  return *this; }
		constexpr EnumFlag& Set(EnumFlag f)   noexcept { m_mask |= f.m_mask;            return *this; }
		constexpr EnumFlag& Unset(E flag)     noexcept { m_mask &= ~ToUnderlying(flag); return *this; }
		constexpr EnumFlag& Unset(EnumFlag f) noexcept { m_mask &= ~f.m_mask;           return *this; }
		constexpr EnumFlag& Toggle(E flag)    noexcept { m_mask ^= ToUnderlying(flag);  return *this; }
		constexpr EnumFlag& Clear()           noexcept { m_mask = 0;                    return *this; }

		[[nodiscard]] constexpr EnumFlag operator|(EnumFlag rhs) const noexcept { return EnumFlag(m_mask | rhs.m_mask); }
		[[nodiscard]] constexpr EnumFlag operator&(EnumFlag rhs) const noexcept { return EnumFlag(m_mask & rhs.m_mask); }
		[[nodiscard]] constexpr EnumFlag operator^(EnumFlag rhs) const noexcept { return EnumFlag(m_mask ^ rhs.m_mask); }
		[[nodiscard]] constexpr EnumFlag operator~()             const noexcept { return EnumFlag(~m_mask);             }

		constexpr EnumFlag& operator|=(EnumFlag rhs) noexcept { m_mask |= rhs.m_mask; return *this; }
		constexpr EnumFlag& operator&=(EnumFlag rhs) noexcept { m_mask &= rhs.m_mask; return *this; }
		constexpr EnumFlag& operator^=(EnumFlag rhs) noexcept { m_mask ^= rhs.m_mask; return *this; }

		[[nodiscard]] constexpr bool operator==(EnumFlag rhs) const noexcept { return m_mask == rhs.m_mask; }

		[[nodiscard]] constexpr UnderlyingType Raw() const noexcept { return m_mask; }

	private:
		[[nodiscard]] static constexpr UnderlyingType ToUnderlying(E value) noexcept
		{
			return static_cast<UnderlyingType>(value);
		}

	private:
		UnderlyingType m_mask{ 0 };
	};
}

template <::koi::EnumFlagType E>
[[nodiscard]] constexpr ::koi::EnumFlag<E> operator|(E lhs, E rhs) noexcept { return ::koi::EnumFlag<E>(lhs) | ::koi::EnumFlag<E>(rhs); }

template <::koi::EnumFlagType E>
[[nodiscard]] constexpr ::koi::EnumFlag<E> operator&(E lhs, E rhs) noexcept { return ::koi::EnumFlag<E>(lhs) & ::koi::EnumFlag<E>(rhs); }

template <::koi::EnumFlagType E>
[[nodiscard]] constexpr ::koi::EnumFlag<E> operator^(E lhs, E rhs) noexcept { return ::koi::EnumFlag<E>(lhs) ^ ::koi::EnumFlag<E>(rhs); }

template <::koi::EnumFlagType E>
[[nodiscard]] constexpr ::koi::EnumFlag<E> operator~(E value) noexcept { return ~::koi::EnumFlag<E>(value); }

template <::koi::EnumFlagType E>
constexpr E& operator|=(E& lhs, E rhs) noexcept
{
	using U = std::underlying_type_t<E>;

	lhs = static_cast<E>(
		static_cast<U>(lhs) |
		static_cast<U>(rhs));

	return lhs;
}

template <::koi::EnumFlagType E>
constexpr E& operator&=(E& lhs, E rhs) noexcept
{
	using U = std::underlying_type_t<E>;

	lhs = static_cast<E>(
		static_cast<U>(lhs) &
		static_cast<U>(rhs));

	return lhs;
}

template <::koi::EnumFlagType E>
constexpr E& operator^=(E& lhs, E rhs) noexcept
{
	using U = std::underlying_type_t<E>;

	lhs = static_cast<E>(
		static_cast<U>(lhs) ^
		static_cast<U>(rhs));

	return lhs;
}
template <::koi::EnumFlagType E>
[[nodiscard]]
constexpr ::koi::EnumFlag<E>
operator|(::koi::EnumFlag<E> lhs, E rhs) noexcept
{
	return lhs | ::koi::EnumFlag<E>(rhs);
}

template <::koi::EnumFlagType E>
constexpr ::koi::EnumFlag<E>&
operator|=(::koi::EnumFlag<E>& lhs, E rhs) noexcept
{
	return lhs |= ::koi::EnumFlag<E>(rhs);
}

template <::koi::EnumFlagType E>
[[nodiscard]]
constexpr ::koi::EnumFlag<E>
operator&(::koi::EnumFlag<E> lhs, E rhs) noexcept
{
	return lhs & ::koi::EnumFlag<E>(rhs);
}

template <::koi::EnumFlagType E>
[[nodiscard]]
constexpr ::koi::EnumFlag<E>
operator^(::koi::EnumFlag<E> lhs, E rhs) noexcept
{
	return lhs ^ ::koi::EnumFlag<E>(rhs);
}
