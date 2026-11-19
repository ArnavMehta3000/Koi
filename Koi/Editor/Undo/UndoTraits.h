#pragma once
#include "Koi/Engine/Core/Reflect.h"
#include <concepts>
#include <utility>

namespace koi::editor
{
	// Specialize with a static Equal(a, b)
	template <typename T>
	struct UndoTraits;

	template <typename T>
	[[nodiscard]] bool UndoEqual(const T& a, const T& b)
	{
		if constexpr (requires { UndoTraits<T>::Equal(a, b); })
		{
			return UndoTraits<T>::Equal(a, b);
		}
		else if constexpr (refl::kIsTransient<T>)
		{
			return true;
		}
		else if constexpr (requires { typename T::value_type; a.has_value(); *a; })  // std::optional
		{
			if (a.has_value() != b.has_value()) return false;
			return !a.has_value() || UndoEqual(*a, *b);
		}
		else if constexpr (std::equality_comparable<T>)
		{
			return a == b;
		}
		else if constexpr (refl::Reflected<T>)
		{
			bool equal = true;
			refl::ForEachMember<T>([&](const refl::FieldInfo&, auto member) { equal = equal && UndoEqual(a.*member, b.*member); });
			return equal;
		}
		else
		{
			static_assert(false, "Give T an operator==, KOI_REFLECT, or an UndoTraits<T>::Equal");
		}
	}

	// Copies into `live` each field where `to` and `from` differ, leaving the others as they are now
	template <refl::Reflected T>
	void UndoCopyChanged(T& live, const T& to, const T& from)
	{
		refl::ForEachMember<T>([&](const refl::FieldInfo&, auto member)
		{
			if (!UndoEqual(to.*member, from.*member)) live.*member = to.*member;
		});
	}
}
