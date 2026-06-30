module;
#include <hlsl++.h>
export module Koi.Core.Transform;

export import Koi.Core.Math;

export namespace koi
{
	struct Transform
	{
		math::float3 Position   { 0.0f, 0.0f, 0.0f };
		math::float3 EulerAngles{ 0.0f, 0.0f, 0.0f };
		math::float3 Scale      { 1.0f, 1.0f, 1.0f };

		[[nodiscard]] math::float4x4 ToMatrix() const;
	};
}
