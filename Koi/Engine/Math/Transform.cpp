#include "Koi/Engine/Math/Transform.h"
#include <array>

namespace koi
{
	float4x4 Transform::ToMatrix() const
	{
		float4x4 S = float4x4::scale(Scale);

		// Extract radian components for the individual rotation calls
		std::array<f32, 3> r;
		store(r.data(), EulerAngles);

		// roll (Z), pitch (X), yaw (Y)
		float4x4 R = float4x4::rotation_z(r[2]) * float4x4::rotation_x(r[0]) * float4x4::rotation_y(r[1]);

		float4x4 T = float4x4::translation(Position);
		return mul(mul(S, R), T);
	}
}
