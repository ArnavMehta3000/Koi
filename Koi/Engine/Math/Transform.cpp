#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/ECS/World.h"

namespace koi
{
	float4x4 Transform::ToMatrix() const
	{
		const float4x4 S = float4x4::scale(Scale);
		const float4x4 R = float4x4(Rotation);
		const float4x4 T = float4x4::translation(Position);

		return mul(mul(S, R), T);
	}
}
