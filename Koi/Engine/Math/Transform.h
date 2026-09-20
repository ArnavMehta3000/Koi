#pragma once
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	struct Transform
	{
		static constexpr std::string_view kKey = "transform";

		Vector3 Position    = Vector3::Zero;
		Quaternion Rotation = Quaternion::Identity;
		Vector3 Scale       = Vector3::One;

		[[nodiscard]] Matrix ToMatrix() const;
	};
}
