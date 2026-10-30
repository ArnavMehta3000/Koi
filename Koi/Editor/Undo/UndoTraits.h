#pragma once
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <rfl/num_fields.hpp>
#include <rfl/to_view.hpp>
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
		else if constexpr (requires { typename T::Type; a.get(); })  // rfl::Skip<>
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
		else if constexpr (ReflectedComponent<T>)
		{
			const auto lhs = rfl::to_view(a);
			const auto rhs = rfl::to_view(b);
			return [&]<int... I>(std::integer_sequence<int, I...>)
			{
				return (UndoEqual(*lhs.template get<I>(), *rhs.template get<I>()) && ...);
			}(std::make_integer_sequence<int, static_cast<int>(rfl::num_fields<T>)>{});
		}
		else
		{
			static_assert(false, "Give T an operator==, kReflect, or an UndoTraits<T>::Equal");
		}
	}

	// Copies into `live` each field where `to` and `from` differ, leaving the others as they are now
	template <ReflectedComponent T>
	void UndoCopyChanged(T& live, const T& to, const T& from)
	{
		const auto dst = rfl::to_view(live);
		const auto lhs = rfl::to_view(to);
		const auto rhs = rfl::to_view(from);
		[&]<int... I>(std::integer_sequence<int, I...>)
		{
			([&]
			{
				if (!UndoEqual(*lhs.template get<I>(), *rhs.template get<I>())) *dst.template get<I>() = *lhs.template get<I>();
			}(), ...);
		}(std::make_integer_sequence<int, static_cast<int>(rfl::num_fields<T>)>{});
	}
}
