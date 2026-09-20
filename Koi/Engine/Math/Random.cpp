#include "Koi/Engine/Math/Random.h"
#include "Koi/Engine/Math/Scalar.h"
#include <random>

namespace koi
{
	namespace
	{
		// Reference output of pcg32_srandom_r(42, 54) from the PCG paper's demo
		constexpr u32 NthOutput(u32 n)
		{
			Random r(42, 54);
			u32 v = 0;
			for (u32 i = 0; i <= n; ++i) v = r.NextU32();
			return v;
		}
		static_assert(NthOutput(0) == 0xa15c02b7u);
		static_assert(NthOutput(1) == 0x7b47f409u);
		static_assert(NthOutput(5) == 0xcbed606eu);
	}

	Random::Random()
	{
		std::random_device device;
		const u64 seed   = (static_cast<u64>(device()) << 32) | device();
		const u64 stream = (static_cast<u64>(device()) << 32) | device();
		*this = Random(seed, stream);
	}

	Vector2 Random::UnitCircle()
	{
		const f32 angle = Range(0.0f, kTwoPi);
		return { std::cos(angle), std::sin(angle) };
	}

	Vector3 Random::UnitSphere()
	{
		const f32 z     = Range(-1.0f, 1.0f);
		const f32 angle = Range(0.0f, kTwoPi);
		const f32 r     = std::sqrt(1.0f - z * z);
		return { r * std::cos(angle), r * std::sin(angle), z };
	}

	Vector3 Random::InsideUnitSphere()
	{
		return UnitSphere() * std::cbrt(NextF32());
	}

	Color Random::RandomColor(f32 alpha)
	{
		return { NextF32(), NextF32(), NextF32(), alpha };
	}
}
