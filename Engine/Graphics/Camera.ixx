module;
export module Koi.Graphics.Camera;

import Koi.Core.Types;
import Koi.Core.Math;

export namespace koi
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

		f32 FovY   = 1.0472f;
		f32 Aspect = 16.0f / 9.0f;
		f32 Near   = 0.1f;
		f32 Far    = 1000.0f;

		void SetView(math::float3 position, math::float3 target,
		             math::float3 worldUp = { 0.0f, 1.0f, 0.0f });
		void SetProjection(f32 fovYRad, f32 aspect, f32 nearZ, f32 farZ);
		void SetAspect(f32 aspect);
	};
}
