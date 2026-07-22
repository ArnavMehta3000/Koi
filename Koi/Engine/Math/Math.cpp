#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Math/MathSerializer.h"
#include "Koi/Engine/ECS/World.h"
#include <DirectXMath.h>  // hacky bridge: use DirectXMath's tested Euler<->quaternion conversions
#include <algorithm>
#include <cmath>
#include <cfloat>
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

	quaternion MathUtils::QuaternionFromEulerZXY(const float3& radians)
	{
		// hlslpp's Euler<->quaternion helpers are awkward here (and operator* on matrices is
		// component-wise, not a real product), so bridge to DirectXMath. XMQuaternionRotationRollPitchYaw
		// applies roll(Z), pitch(X), yaw(Y) - i.e. ZXY - matching our Transform convention
		interop::float3 r(radians);
		DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(r.x, r.y, r.z); // (pitch=X, yaw=Y, roll=Z)
		DirectX::XMFLOAT4 f;
		DirectX::XMStoreFloat4(&f, q);
		return quaternion(f.x, f.y, f.z, f.w);
	}

	float3 MathUtils::ToEulerZXY(const quaternion& q)
	{
		// DirectXMath has no quaternion->Euler; this is DirectXTK SimpleMath's Quaternion::ToEuler,
		// the exact inverse of XMQuaternionRotationRollPitchYaw. Returns (pitch=X, yaw=Y, roll=Z) radians
		const interop::float4 c(q.xyzw);
		const f32 xx = c.x * c.x, yy = c.y * c.y, zz = c.z * c.z;

		const f32 m31 = 2.0f * c.x * c.z + 2.0f * c.y * c.w;
		const f32 m32 = 2.0f * c.y * c.z - 2.0f * c.x * c.w;
		const f32 m33 = 1.0f - 2.0f * xx - 2.0f * yy;

		const f32 cy = std::sqrt(m33 * m33 + m31 * m31);
		const f32 cx = std::atan2(-m32, cy);

		if (cy > 16.0f * FLT_EPSILON)
		{
			const f32 m12 = 2.0f * c.x * c.y + 2.0f * c.z * c.w;
			const f32 m22 = 1.0f - 2.0f * xx - 2.0f * zz;
			return float3(cx, std::atan2(m31, m33), std::atan2(m12, m22));
		}

		// Gimbal lock: roll and yaw are coupled - fold everything into roll.
		const f32 m11 = 1.0f - 2.0f * yy - 2.0f * zz;
		const f32 m21 = 2.0f * c.x * c.y - 2.0f * c.z * c.w;
		return float3(cx, 0.0f, std::atan2(-m21, m11));
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
