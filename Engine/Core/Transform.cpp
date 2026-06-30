module;
#include <hlsl++.h>
module Koi.Core.Transform;

import Koi.Core.Math;

namespace koi
{
	math::float4x4 Transform::ToMatrix() const
	{
		using namespace math;

		float4x4 S = float4x4::scale(Scale);

		float r[3]{};
		store(r, EulerAngles);

		// roll (Z), pitch (X), yaw (Y)
		float4x4 R = float4x4::rotation_z(r[2]) * float4x4::rotation_x(r[0]) * float4x4::rotation_y(r[1]);

		float4x4 T = float4x4::translation(Position);

		return S * R * T;
	}
}
