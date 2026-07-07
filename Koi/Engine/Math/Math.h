// hlslpp math types + engine math helpers
#pragma once

#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/Types.h"

#if KOI_DEBUG
	#define HLSLPP_ASSERT
#endif

#include <hlsl++.h>
#include <algorithm>
#include <cmath>

namespace koi::math
{
	// Pull hlslpp types into Koi's math namespace
	using namespace hlslpp;

	struct AABB
	{
		float3 Min{};
		float3 Max{};

		void Expand(const float3 point)
		{
			Min = min(Min, point);
			Max = max(Max, point);
		}

		void Expand(const AABB& other)
		{
			Expand(other.Min);
			Expand(other.Max);
		}

		[[nodiscard]] float3 Center() const
		{
			return (Min + Max) * 0.5f;
		}

		// Radius of a sphere that encloses this AABB, centered at Center()
		[[nodiscard]] float Radius() const
		{
			const float3 extent = (Max - Min) * 0.5f;
			f32 e[3]{};
			store(e, extent);
			return std::max(std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]), 0.001f);
		}
	};
}
