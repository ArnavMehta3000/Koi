// Full hlslpp include
#pragma once
#include "MathTypes.h"
#include <hlsl++.h>

namespace koi::math
{
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
	};
}
