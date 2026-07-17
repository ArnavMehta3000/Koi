#pragma once
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	class World;

	struct Transform
	{
		float3 Position   { 0.0f, 0.0f, 0.0f };
		float3 EulerAngles{ 0.0f, 0.0f, 0.0f };  // radians, ZXY order
		float3 Scale      { 1.0f, 1.0f, 1.0f };

		[[nodiscard]] float4x4 ToMatrix() const;
	};

	// Test new transform struct
	struct TF
	{
		float3 Position;
		quaternion Orientation;
		float3 Scale;

		// Configure the ECS world for this component, called once internally
		static void ConfigureECS(World& world);
	};

	struct WorldTF: TF {};  // Tagged transforms
	struct LocalTF: TF {};  // Tagged transforms
}
