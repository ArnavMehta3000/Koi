#pragma once
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	class World;

	struct Transform
	{
		float3     Position{ 0.0f, 0.0f, 0.0f };
		quaternion Rotation = quaternion::identity();
		float3     Scale   { 1.0f, 1.0f, 1.0f };

		[[nodiscard]] float4x4 ToMatrix() const;
	};
}
