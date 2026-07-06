#pragma once
#include "Core/Types.h"
#include "Core/MathTypes.h"
#include "Core/Transform.h"

namespace koi
{
	struct Camera
	{
		math::float4x4 View{};
		math::float4x4 Projection{};
		math::float4x4 ViewProjection{};

		math::float3 Position{ 0.0f, 0.0f,  0.0f };
		math::float3 Forward { 0.0f, 0.0f,  1.0f };
		math::float3 Right   { 1.0f, 0.0f,  0.0f };
		math::float3 Up      { 0.0f, 1.0f,  0.0f };

		// Projection parameters (kept for aspect-ratio updates)
		f32 FovY   = 1.0472f;        // 60 degrees in radians
		f32 Aspect = 16.0f / 9.0f;
		f32 Near   = 0.1f;
		f32 Far    = 1000.0f;

		// Recomputes View, basis vectors, and ViewProjection
		void SetView(math::float3 position, math::float3 target,
		             math::float3 worldUp = { 0.0f, 1.0f, 0.0f });

		void SetTransform(const Transform& transform, math::float3 worldUp = { 0.0f, 1.0f, 0.0f });

		// Recomputes Projection and ViewProjection. D3D depth range [0,1]
		void SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ);

		// Recomputes projection with a new aspect ratio, keeps other params
		void SetAspect(f32 aspect);
	};
}
