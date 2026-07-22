#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	void Camera::SetView(float3 position, float3 target, float3 worldUp)
	{
		Position = position;
		Forward  = normalize(target - position);
		Right    = normalize(cross(worldUp, Forward)); // left-handed: right = up × forward
		Up       = cross(Forward, Right);

		View           = float4x4::look_at(position, target, worldUp);
		ViewProjection = mul(View, Projection);
	}

	void Camera::SetTransform(const Transform& transform, float3 worldUp)
	{
		float4x4 R = float4x4(transform.Rotation);
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

		Projection = float4x4::perspective(projection(
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
