#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	void Camera::SetView(math::float3 position, math::float3 target, math::float3 worldUp)
	{
		using namespace math;

		Position = position;
		Forward  = normalize(target - position);
		Right    = normalize(cross(worldUp, Forward)); // left-handed: right = up × forward
		Up       = cross(Forward, Right);

		View           = math::float4x4::look_at(position, target, worldUp);
		ViewProjection = mul(View, Projection);
	}

	void Camera::SetTransform(const Transform& transform, math::float3 worldUp)
	{
		using namespace math;

		float r[3]{};
		store(r, transform.EulerAngles);

		float4x4 R = float4x4::rotation_z(r[2]) * float4x4::rotation_x(r[0]) * float4x4::rotation_y(r[1]);
		float3 forward = normalize(mul(float4(0.0f, 0.0f, 1.0f, 0.0f), R).xyz);

		SetView(transform.Position, transform.Position + forward, worldUp);
	}

	void Camera::SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ)
	{
		using namespace hlslpp;

		FovY   = fovYRad;
		Aspect = aspect;
		Near   = nearZ;
		Far    = farZ;

		Projection = math::float4x4::perspective(projection(
			frustum::field_of_view_y(fovYRad, aspect, nearZ, farZ),
			zclip::zero  // D3D depth range [0, 1]
		));

		ViewProjection = mul(View, Projection);
	}

	void Camera::SetAspect(f32 aspect)
	{
		SetProjection(FovY, aspect, Near, Far);
	}
}
