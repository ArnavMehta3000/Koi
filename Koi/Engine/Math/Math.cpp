#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/ECS/World.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <array>

namespace koi
{

	void AABB::Expand(const Vector3& point)
	{
		Min = Vector3::Min(Min, point);
		Max = Vector3::Max(Max, point);
	}

	void AABB::Expand(const AABB& other)
	{
		Expand(other.Min);
		Expand(other.Max);
	}

	Vector3 AABB::Center() const
	{
		return (Min + Max) * 0.5f;
	}

	f32 AABB::Radius() const
	{
		const Vector3 extent = (Max - Min) * 0.5f;
		return std::max(
			std::sqrt(
				extent.x * extent.x
				+ extent.y * extent.y
				+ extent.z * extent.z),
			0.001f);
	}
}
