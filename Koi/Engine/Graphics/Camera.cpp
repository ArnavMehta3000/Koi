#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Math/Math.h"
#include <algorithm>
#include <cmath>

namespace koi
{
	void Camera::SetView(Vector3 position, Vector3 target, Vector3 worldUp)
	{
		Vector3 forward = target - position;
		if (forward.LengthSquared() < 1e-12f) return;
		forward.Normalize();

		Vector3 right = forward.Cross(worldUp);
		if (right.LengthSquared() < 1e-8f)
		{
			right = forward.Cross(std::abs(forward.z) < 0.99f ? Vector3::Backward : Vector3::Right);
		}
		right.Normalize();

		SetBasis(position, forward, right, right.Cross(forward));
	}

	void Camera::SetTransform(const Transform& transform)
	{
		const Matrix R = Matrix::CreateFromQuaternion(math::SafeNormalize(transform.Rotation));
		SetBasis(transform.Position, R.Forward(), R.Right(), R.Up());
	}

	void Camera::SetBasis(Vector3 position, Vector3 forward, Vector3 right, Vector3 up)
	{
		Position = position;
		Forward  = forward;
		Right    = right;
		Up       = up;

		// The inverse of the camera's rigid world matrix
		View = Matrix(
			Right.x, Up.x, -Forward.x, 0.0f,
			Right.y, Up.y, -Forward.y, 0.0f,
			Right.z, Up.z, -Forward.z, 0.0f,
			-Right.Dot(Position), -Up.Dot(Position), Forward.Dot(Position), 1.0f);
		ViewProjection = View * Projection;
	}

	void Camera::SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ)
	{
		// Every one of these trips an assert in the projection builder when zero or equal
		FovY   = std::clamp(fovYRad, 0.01f, 3.13f);
		Aspect = aspect > 1e-4f ? aspect : 1.0f;
		Near   = std::max(nearZ, 1e-4f);
		Far    = std::max(farZ, Near + 1e-3f);

		Projection = Matrix::CreatePerspectiveFieldOfView(FovY, Aspect, Near, Far);
		ViewProjection = View * Projection;
	}

	void Camera::SetAspect(f32 aspect)
	{
		if (Aspect == aspect) return;
		SetProjection(FovY, aspect, Near, Far);
	}
}
