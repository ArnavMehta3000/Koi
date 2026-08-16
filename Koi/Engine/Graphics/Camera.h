#pragma once
#include "Koi/Engine/Math/Transform.h"

namespace koi
{
	struct Camera
	{
		Matrix View           = Matrix::Identity;
		Matrix Projection     = Matrix::Identity;
		Matrix ViewProjection = Matrix::Identity;

		Vector3 Position = Vector3::Zero;
		Vector3 Forward  = Vector3::Forward;
		Vector3 Right    = Vector3::Right;
		Vector3 Up       = Vector3::Up;

		// Projection parameters (kept for aspect-ratio updates)
		f32 FovY   = 1.0472f;        // 60 degrees in radians
		f32 Aspect = 16.0f / 9.0f;
		f32 Near   = 0.1f;
		f32 Far    = 1000.0f;

		// Recomputes View, basis vectors, and ViewProjection
		void SetView(Vector3 position, Vector3 target,
		             Vector3 worldUp = Vector3::Up);

		void SetTransform(const Transform& transform, Vector3 worldUp = Vector3::Up);

		// Recomputes Projection and ViewProjection. D3D depth range [0,1]
		void SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ);

		// Recomputes projection with a new aspect ratio, keeps other params
		void SetAspect(f32 aspect);
	};
}
