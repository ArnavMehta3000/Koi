#pragma once
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include <algorithm>
#include <cmath>

namespace koi::internal
{
	// Box3D has no scale, so a Collider is resolved against its entity's scale once, for both the shape and its debug draw
	struct ScaledCollider
	{
		Vector3 Center;
		Vector3 HalfExtents;
		f32     Radius     = 0.0f;
		f32     HalfHeight = 0.0f;
	};

	[[nodiscard]] inline ScaledCollider ScaleCollider(const Collider& collider, const Vector3& entityScale)
	{
		constexpr f32 kMinExtent = 0.005f;

		const Vector3 scale(std::abs(entityScale.x), std::abs(entityScale.y), std::abs(entityScale.z));
		const Vector3 half = collider.HalfExtents * scale;

		f32 radius = collider.Radius * std::max({ scale.x, scale.y, scale.z });
		if (collider.Shape == ColliderShape::Capsule)
		{
			radius = collider.Radius * std::max(scale.x, scale.z);
		}

		return ScaledCollider
		{
			.Center      = collider.Offset * scale,
			.HalfExtents = Vector3(std::max(half.x, kMinExtent), std::max(half.y, kMinExtent), std::max(half.z, kMinExtent)),
			.Radius      = std::max(radius, kMinExtent),
			.HalfHeight  = collider.HalfHeight * scale.y,
		};
	}
}
