module;
#include "Core/Config.h"
#if KOI_DEBUG
	#define HLSLPP_ASSERT
#endif
#include <hlsl++.h>

export module Koi.Core.Math;

export namespace koi::math
{
	// Bring hlslpp namespace into koi::math
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
	};
}
