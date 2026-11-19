#pragma once
#include "Koi/Engine/Core/Reflect.h"
#include "Koi/Engine/Core/Types.h"
#include <concepts>
#include <functional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

namespace koi::utils
{
	inline constexpr u64 kFnvOffset = 14695981039346656037ull;
	inline constexpr u64 kFnvPrime  = 1099511628211ull;

	constexpr void FnvFeed(u64& hash, std::string_view text)
	{
		for (const char c : text)
		{
			hash ^= static_cast<u64>(static_cast<u8>(c));
			hash *= kFnvPrime;
		}
	}

	[[nodiscard]] constexpr u64 Fnv1a(std::string_view text)
	{
		u64 hash = kFnvOffset;
		FnvFeed(hash, text);
		return hash;
	}

	constexpr void HashCombine(u64& seed, u64 value)
	{
		seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
	}

	template <typename T>
	concept SelfHashed = requires(const T& value) { { value.GetHash() } -> std::convertible_to<u64>; };

	// Walks a KOI_REFLECT struct's fields, recursing into nested structs and ranges
	template <typename T>
	void HashFields(u64& seed, const T& value)
	{
		if constexpr (SelfHashed<T>)
			HashCombine(seed, value.GetHash());
		else if constexpr (std::is_enum_v<T>)
			HashCombine(seed, static_cast<u64>(std::to_underlying(value)));
		else if constexpr (std::is_arithmetic_v<T>)
			HashCombine(seed, std::hash<T>{}(value));
		else if constexpr (std::ranges::range<T>)
			for (const auto& element : value) HashFields(seed, element);
		else if constexpr (refl::Reflected<T>)
			refl::ForEachField(value, [&seed]<typename F>(const refl::FieldInfo&, const F& field) { if constexpr (!refl::kIsTransient<F>) HashFields(seed, field); });
		else
			static_assert(false, "Give T a GetHash() or KOI_REFLECT");
	}

	// Hashes every field, so it agrees with a defaulted operator==
	struct FieldHash
	{
		template <typename T>
		[[nodiscard]] size_t operator()(const T& value) const noexcept
		{
			u64 seed = kFnvOffset;
			HashFields(seed, value);
			return static_cast<size_t>(seed);
		}
	};
}
