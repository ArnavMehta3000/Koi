#pragma once
#include "Koi/Engine/Math/MathTypes.h"
#include <cmath>
#include <numbers>

namespace koi
{
	inline constexpr f32 kPi     = std::numbers::pi_v<f32>;
	inline constexpr f32 kTwoPi  = 2.0f * kPi;
	inline constexpr f32 kHalfPi = 0.5f * kPi;

	[[nodiscard]] inline constexpr f32 DegToRad(f32 degrees) { return degrees * (kPi / 180.0f); }
	[[nodiscard]] inline constexpr f32 RadToDeg(f32 radians) { return radians * (180.0f / kPi); }

	[[nodiscard]] inline constexpr f32 Square(f32 v) { return v * v; }
	[[nodiscard]] inline constexpr f32 Sign(f32 v)   { return v < 0.0f ? -1.0f : 1.0f; }

	[[nodiscard]] inline constexpr f32 Saturate(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
	[[nodiscard]] inline constexpr f32 Clamp01(f32 v)  { return Saturate(v); }

	[[nodiscard]] inline constexpr bool Approximately(f32 a, f32 b, f32 epsilon = 1e-5f)
	{
		const f32 d = a - b;
		return (d < 0.0f ? -d : d) <= epsilon;
	}

	[[nodiscard]] inline constexpr f32 Lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

	// t such that Lerp(a, b, t) == v; unclamped
	[[nodiscard]] inline constexpr f32 InverseLerp(f32 a, f32 b, f32 v)
	{
		return Approximately(a, b) ? 0.0f : (v - a) / (b - a);
	}

	[[nodiscard]] inline constexpr f32 Remap(f32 v, f32 inMin, f32 inMax, f32 outMin, f32 outMax)
	{
		return Lerp(outMin, outMax, InverseLerp(inMin, inMax, v));
	}

	[[nodiscard]] inline constexpr f32 RemapClamped(f32 v, f32 inMin, f32 inMax, f32 outMin, f32 outMax)
	{
		return Lerp(outMin, outMax, Saturate(InverseLerp(inMin, inMax, v)));
	}

	[[nodiscard]] inline constexpr f32 MoveTowards(f32 current, f32 target, f32 maxDelta)
	{
		const f32 delta = target - current;
		if (delta <= maxDelta && delta >= -maxDelta) return target;
		return current + Sign(delta) * maxDelta;
	}

	[[nodiscard]] inline Vector3 MoveTowards(const Vector3& current, const Vector3& target, f32 maxDelta)
	{
		const Vector3 delta = target - current;
		const f32 dist = delta.Length();
		if (dist <= maxDelta || dist <= 1e-6f) return target;
		return current + delta * (maxDelta / dist);
	}

	// Frame-rate independent lerp: fraction 1 - e^-k of the remaining distance is covered per second
	[[nodiscard]] inline f32 ExpDecay(f32 a, f32 b, f32 decay, f32 dt)
	{
		return b + (a - b) * std::exp(-decay * dt);
	}

	[[nodiscard]] inline Vector3 ExpDecay(const Vector3& a, const Vector3& b, f32 decay, f32 dt)
	{
		return b + (a - b) * std::exp(-decay * dt);
	}

	// Critically damped spring toward target; velocity persists between calls
	[[nodiscard]] inline f32 SmoothDamp(f32 current, f32 target, f32& velocity, f32 smoothTime, f32 dt)
	{
		smoothTime = smoothTime > 1e-4f ? smoothTime : 1e-4f;
		const f32 omega = 2.0f / smoothTime;
		const f32 x     = omega * dt;
		const f32 damp  = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

		const f32 change = current - target;
		const f32 temp   = (velocity + omega * change) * dt;
		velocity = (velocity - omega * temp) * damp;

		f32 result = target + (change + temp) * damp;
		if ((target - current > 0.0f) == (result > target))
		{
			result   = target;
			velocity = (result - target) / dt;
		}
		return result;
	}

	[[nodiscard]] inline Vector3 SmoothDamp(const Vector3& current, const Vector3& target,Vector3& velocity, f32 smoothTime, f32 dt)
	{
		return
		{
			SmoothDamp(current.x, target.x, velocity.x, smoothTime, dt),
			SmoothDamp(current.y, target.y, velocity.y, smoothTime, dt),
			SmoothDamp(current.z, target.z, velocity.z, smoothTime, dt),
		};
	}

	// Wraps v into [min, max)
	[[nodiscard]] inline f32 Wrap(f32 v, f32 min, f32 max)
	{
		const f32 range = max - min;
		if (range <= 0.0f) return min;
		const f32 r = std::fmod(v - min, range);
		return min + (r < 0.0f ? r + range : r);
	}

	// Bounces v between 0 and length
	[[nodiscard]] inline f32 PingPong(f32 v, f32 length)
	{
		const f32 w = Wrap(v, 0.0f, length * 2.0f);
		return length - std::abs(w - length);
	}
}
