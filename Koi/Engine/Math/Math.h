// hlslpp math types + engine math helpers
#pragma once

#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/Types.h"

#if KOI_DEBUG
	#define HLSLPP_ASSERT
#endif

#include <hlsl++.h>

namespace koi
{
	// Pull hlslpp types into Koi's namespace
	using namespace hlslpp;
	class World;

	struct AABB
	{
		float3 Min{};
		float3 Max{};

		void Expand(const float3 point);
		void Expand(const AABB& other);

		[[nodiscard]] float3 Center() const;

		// Radius of a sphere that encloses this AABB, centered at Center()
		[[nodiscard]] f32 Radius() const;
	};

	using DecomposedTransform = std::tuple<float3, quaternion, float3>;  // pos, rot, scale

	class MathUtils
	{
	public:
		static DecomposedTransform DecomposeTransform(const float4x4& m);

		// ZXY Euler angles (radians) <-> quaternion, via DirectXMath (XMQuaternionRotationRollPitchYaw)
		static quaternion QuaternionFromEulerZXY(const float3& radians);
		static float3     ToEulerZXY(const quaternion& q);

		// Aligns a value to the nearest multiple of 256
		template <typename T>
		static T AlignGPU256(T value) { return (value + static_cast<T>(255)) & ~static_cast<T>(255); }
	};
}
