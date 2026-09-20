#pragma once

#include "Koi/Engine/Math/MathTypes.h"
#include <cmath>
#include <span>
#include <utility>

namespace koi
{
	// PCG32 (XSH-RR)
	class Random
	{
	public:
		Random();  // seeded from the OS
		explicit constexpr Random(u64 seed, u64 stream = 0)
		{
			m_inc   = (stream << 1u) | 1u;
			m_state = 0;
			NextU32();
			m_state += seed;
			NextU32();
		}

		constexpr u32 NextU32()
		{
			const u64 old = m_state;
			m_state = old * 6364136223846793005ull + m_inc;
			const u32 xorshifted = static_cast<u32>(((old >> 18u) ^ old) >> 27u);
			const u32 rot        = static_cast<u32>(old >> 59u);
			return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
		}

		constexpr u64 NextU64() { return (static_cast<u64>(NextU32()) << 32) | NextU32(); }

		// [0, 1)
		constexpr f32 NextF32() { return static_cast<f32>(NextU32() >> 8) * 0x1.0p-24f; }

		// [min, max]
		constexpr i32 Range(i32 min, i32 max)
		{
			if (max <= min) return min;
			const u32 span = static_cast<u32>(max - min) + 1u;
			return min + static_cast<i32>((static_cast<u64>(NextU32()) * span) >> 32);
		}

		// [min, max)
		constexpr f32 Range(f32 min, f32 max) { return min + (max - min) * NextF32(); }

		constexpr bool Chance(f32 probability) { return NextF32() < probability; }
		constexpr f32  Sign()                  { return (NextU32() & 1u) ? 1.0f : -1.0f; }

		template<typename T>
		T& Element(std::span<T> items) { return items[static_cast<size_t>(Range(0, static_cast<i32>(items.size()) - 1))]; }

		template<typename T>
		void Shuffle(std::span<T> items)
		{
			for (size_t i = items.size(); i > 1; --i)
			{
				std::swap(items[i - 1], items[static_cast<size_t>(Range(0, static_cast<i32>(i) - 1))]);
			}
		}

		Vector2 UnitCircle();
		Vector3 UnitSphere();
		Vector3 InsideUnitSphere();
		Color   RandomColor(f32 alpha = 1.0f);

		[[nodiscard]] constexpr u64 GetState() const { return m_state; }

	private:
		u64 m_state = 0;
		u64 m_inc   = 1;
	};
}
