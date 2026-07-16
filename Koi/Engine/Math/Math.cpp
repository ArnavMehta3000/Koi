#include "Koi/Engine/Math/Math.h"
#include <algorithm>
#include <cmath>
#include <array>

namespace koi
{
	DecomposedTransform MathUtils::DecomposeTransform(const float4x4& m)
	{
		DecomposedTransform result;

		std::get<0>(result) = m[3].xyz;

		std::array rows{ m[0].xyz, m[1].xyz, m[2].xyz };
		std::array scale{ length(rows[0]), length(rows[1]), length(rows[2]) };

		if (static_cast<f32>(determinant(float3x3(rows[0], rows[1], rows[2]))) < 0.0f)
			scale[0] = -scale[0];

		std::get<2>(result) = float3(scale[0], scale[1], scale[2]);

		for (i32 i = 0; i < 3; ++i)
			rows[i] = rows[i] / scale[i];

		std::get<1>(result) = quaternion(float3x3(rows[0], rows[1], rows[2]));
		return result;
	}

	void AABB::Expand(const float3 point)
	{
		Min = min(Min, point);
		Max = max(Max, point);
	}

	void AABB::Expand(const AABB& other)
	{
		Expand(other.Min);
		Expand(other.Max);
	}

	[[nodiscard]] float3 AABB::Center() const
	{
		return (Min + Max) * 0.5f;
	}

	// Radius of a sphere that encloses this AABB, centered at Center()
	[[nodiscard]] f32 AABB::Radius() const
	{
		const float3 extent = (Max - Min) * 0.5f;
		f32 e[3]{};
		store(e, extent);
		return std::max(std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]), 0.001f);
	}
}
