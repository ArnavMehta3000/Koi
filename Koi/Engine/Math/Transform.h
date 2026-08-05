#pragma once
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	class World;

	struct Transform
	{
		Vector3 Position    = Vector3::Zero;
		Quaternion Rotation = Quaternion::Identity;
		Vector3 Scale       = Vector3::One;

		[[nodiscard]] Matrix ToMatrix() const;
	};
}
