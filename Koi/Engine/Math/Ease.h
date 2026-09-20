// Penner easing curves over t in [0, 1]
#pragma once

#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Math/Scalar.h"
#include <array>
#include <cmath>
#include <optional>
#include <string_view>

namespace koi
{
	enum class EaseType : u8
	{
		Linear,
		InQuad,    OutQuad,    InOutQuad,
		InCubic,   OutCubic,   InOutCubic,
		InQuart,   OutQuart,   InOutQuart,
		InSine,    OutSine,    InOutSine,
		InExpo,    OutExpo,    InOutExpo,
		InBack,    OutBack,    InOutBack,
		InElastic, OutElastic, InOutElastic,
		InBounce,  OutBounce,  InOutBounce,
		Count
	};

	// Indexed by EaseType
	inline constexpr std::array<std::string_view, static_cast<size_t>(EaseType::Count)> kEaseTypeNames =
	{
		"Linear",
		"InQuad",    "OutQuad",    "InOutQuad",
		"InCubic",   "OutCubic",   "InOutCubic",
		"InQuart",   "OutQuart",   "InOutQuart",
		"InSine",    "OutSine",    "InOutSine",
		"InExpo",    "OutExpo",    "InOutExpo",
		"InBack",    "OutBack",    "InOutBack",
		"InElastic", "OutElastic", "InOutElastic",
		"InBounce",  "OutBounce",  "InOutBounce",
	};

	[[nodiscard]] constexpr std::string_view ToString(EaseType type)
	{
		const size_t i = static_cast<size_t>(type);
		return i < kEaseTypeNames.size() ? kEaseTypeNames[i] : "Linear";
	}

	[[nodiscard]] constexpr std::optional<EaseType> EaseTypeFromString(std::string_view name)
	{
		for (size_t i = 0; i < kEaseTypeNames.size(); ++i)
		{
			if (kEaseTypeNames[i] == name) return static_cast<EaseType>(i);
		}
		return std::nullopt;
	}

	[[nodiscard]] constexpr f32 EaseLinear(f32 t) { return t; }

	[[nodiscard]] constexpr f32 EaseInQuad(f32 t)    { return t * t; }
	[[nodiscard]] constexpr f32 EaseOutQuad(f32 t)   { return 1.0f - (1.0f - t) * (1.0f - t); }
	[[nodiscard]] constexpr f32 EaseInOutQuad(f32 t) { return t < 0.5f ? 2.0f * t * t : 1.0f - Square(-2.0f * t + 2.0f) * 0.5f; }

	[[nodiscard]] constexpr f32 EaseInCubic(f32 t)    { return t * t * t; }
	[[nodiscard]] constexpr f32 EaseOutCubic(f32 t)   { const f32 u = 1.0f - t; return 1.0f - u * u * u; }
	[[nodiscard]] constexpr f32 EaseInOutCubic(f32 t) { const f32 u = -2.0f * t + 2.0f; return t < 0.5f ? 4.0f * t * t * t : 1.0f - u * u * u * 0.5f; }

	[[nodiscard]] constexpr f32 EaseInQuart(f32 t)    { return t * t * t * t; }
	[[nodiscard]] constexpr f32 EaseOutQuart(f32 t)   { const f32 u = 1.0f - t; return 1.0f - u * u * u * u; }
	[[nodiscard]] constexpr f32 EaseInOutQuart(f32 t) { const f32 u = -2.0f * t + 2.0f; return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - u * u * u * u * 0.5f; }

	[[nodiscard]] inline f32 EaseInSine(f32 t)    { return 1.0f - std::cos(t * kHalfPi); }
	[[nodiscard]] inline f32 EaseOutSine(f32 t)   { return std::sin(t * kHalfPi); }
	[[nodiscard]] inline f32 EaseInOutSine(f32 t) { return -(std::cos(kPi * t) - 1.0f) * 0.5f; }

	[[nodiscard]] inline f32 EaseInExpo(f32 t)  { return t <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * t - 10.0f); }
	[[nodiscard]] inline f32 EaseOutExpo(f32 t) { return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t); }
	[[nodiscard]] inline f32 EaseInOutExpo(f32 t)
	{
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return t < 0.5f ? std::pow(2.0f, 20.0f * t - 10.0f) * 0.5f : (2.0f - std::pow(2.0f, -20.0f * t + 10.0f)) * 0.5f;
	}

	[[nodiscard]] constexpr f32 EaseInBack(f32 t)
	{
		constexpr f32 c1 = 1.70158f, c3 = c1 + 1.0f;
		return c3 * t * t * t - c1 * t * t;
	}
	[[nodiscard]] constexpr f32 EaseOutBack(f32 t)
	{
		constexpr f32 c1 = 1.70158f, c3 = c1 + 1.0f;
		const f32 u = t - 1.0f;
		return 1.0f + c3 * u * u * u + c1 * u * u;
	}
	[[nodiscard]] constexpr f32 EaseInOutBack(f32 t)
	{
		constexpr f32 c2 = 1.70158f * 1.525f;
		const f32 a = 2.0f * t, b = 2.0f * t - 2.0f;
		return t < 0.5f ? (a * a * ((c2 + 1.0f) * a - c2)) * 0.5f : (b * b * ((c2 + 1.0f) * b + c2) + 2.0f) * 0.5f;
	}

	[[nodiscard]] inline f32 EaseInElastic(f32 t)
	{
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * (kTwoPi / 3.0f));
	}
	[[nodiscard]] inline f32 EaseOutElastic(f32 t)
	{
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (kTwoPi / 3.0f)) + 1.0f;
	}
	[[nodiscard]] inline f32 EaseInOutElastic(f32 t)
	{
		if (t <= 0.0f) return 0.0f;
		if (t >= 1.0f) return 1.0f;
		const f32 s = std::sin((20.0f * t - 11.125f) * (kTwoPi / 4.5f));
		return t < 0.5f ? -(std::pow(2.0f, 20.0f * t - 10.0f) * s) * 0.5f : (std::pow(2.0f, -20.0f * t + 10.0f) * s) * 0.5f + 1.0f;
	}

	[[nodiscard]] constexpr f32 EaseOutBounce(f32 t)
	{
		constexpr f32 n1 = 7.5625f, d1 = 2.75f;
		if (t < 1.0f / d1)  return n1 * t * t;
		if (t < 2.0f / d1)  { t -= 1.5f / d1;   return n1 * t * t + 0.75f; }
		if (t < 2.5f / d1)  { t -= 2.25f / d1;  return n1 * t * t + 0.9375f; }
		t -= 2.625f / d1;   return n1 * t * t + 0.984375f;
	}
	[[nodiscard]] constexpr f32 EaseInBounce(f32 t)    { return 1.0f - EaseOutBounce(1.0f - t); }
	[[nodiscard]] constexpr f32 EaseInOutBounce(f32 t)
	{
		return t < 0.5f ? (1.0f - EaseOutBounce(1.0f - 2.0f * t)) * 0.5f : (1.0f + EaseOutBounce(2.0f * t - 1.0f)) * 0.5f;
	}

	[[nodiscard]] inline f32 Ease(EaseType type, f32 t)
	{
		t = Saturate(t);
		switch (type)
		{
			case EaseType::Linear:       return EaseLinear(t);
			case EaseType::InQuad:       return EaseInQuad(t);
			case EaseType::OutQuad:      return EaseOutQuad(t);
			case EaseType::InOutQuad:    return EaseInOutQuad(t);
			case EaseType::InCubic:      return EaseInCubic(t);
			case EaseType::OutCubic:     return EaseOutCubic(t);
			case EaseType::InOutCubic:   return EaseInOutCubic(t);
			case EaseType::InQuart:      return EaseInQuart(t);
			case EaseType::OutQuart:     return EaseOutQuart(t);
			case EaseType::InOutQuart:   return EaseInOutQuart(t);
			case EaseType::InSine:       return EaseInSine(t);
			case EaseType::OutSine:      return EaseOutSine(t);
			case EaseType::InOutSine:    return EaseInOutSine(t);
			case EaseType::InExpo:       return EaseInExpo(t);
			case EaseType::OutExpo:      return EaseOutExpo(t);
			case EaseType::InOutExpo:    return EaseInOutExpo(t);
			case EaseType::InBack:       return EaseInBack(t);
			case EaseType::OutBack:      return EaseOutBack(t);
			case EaseType::InOutBack:    return EaseInOutBack(t);
			case EaseType::InElastic:    return EaseInElastic(t);
			case EaseType::OutElastic:   return EaseOutElastic(t);
			case EaseType::InOutElastic: return EaseInOutElastic(t);
			case EaseType::InBounce:     return EaseInBounce(t);
			case EaseType::OutBounce:    return EaseOutBounce(t);
			case EaseType::InOutBounce:  return EaseInOutBounce(t);
			case EaseType::Count:        break;
		}
		return t;
	}
}
