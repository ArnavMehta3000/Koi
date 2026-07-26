#include "Koi/Engine/Graphics/Camera.h"

namespace koi
{
	void Camera::SetView(Vector3 position, Vector3 target, Vector3 worldUp)
	{
		Position = position;
		(target - position).Normalize(Forward);
		Forward.Cross(worldUp).Normalize(Right);
		Right.Cross(Forward, Up);

		View = Matrix::CreateLookAt(position, target, worldUp);
		ViewProjection = View * Projection;
	}

	void Camera::SetTransform(const Transform& transform, Vector3 worldUp)
	{
		const Matrix R = Matrix::CreateFromQuaternion(transform.Rotation);
		Vector3 forward = R.Forward();
		forward.Normalize();

		SetView(transform.Position, transform.Position + forward, worldUp);
	}

	void Camera::SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ)
	{
		FovY   = fovYRad;
		Aspect = aspect;
		Near   = nearZ;
		Far    = farZ;

		Projection = Matrix::CreatePerspectiveFieldOfView(FovY, Aspect, Near, Far);
		ViewProjection = View * Projection;
	}

	void Camera::SetAspect(f32 aspect)
	{
		if (Aspect == aspect) return;
		SetProjection(FovY, aspect, Near, Far);
	}
}
