module;
#include <hlsl++.h>
module Koi.Graphics.Camera;

import Koi.Core.Types;
import Koi.Core.Math;

namespace koi
{
	void Camera::SetView(math::float3 position, math::float3 target, math::float3 worldUp)
	{
		using namespace math;
		Position       = position;
		Forward        = normalize(target - position);
		Right          = normalize(cross(worldUp, Forward));
		Up             = cross(Forward, Right);
		View           = math::float4x4::look_at(position, target, worldUp);
		ViewProjection = mul(View, Projection);
	}

	void Camera::SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ)
	{
		using namespace hlslpp;
		FovY   = fovYRad;
		Aspect = aspect;
		Near   = nearZ;
		Far    = farZ;
		Projection     = math::float4x4::perspective(projection(
			frustum::field_of_view_y(fovYRad, aspect, nearZ, farZ),
			zclip::zero
		));
		ViewProjection = mul(View, Projection);
	}

	void Camera::SetAspect(f32 aspect)
	{
		SetProjection(FovY, aspect, Near, Far);
	}
}
