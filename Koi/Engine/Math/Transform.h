#pragma once
#include "Koi/Engine/Math/Math.h"

namespace koi
{
	struct Transform
	{
		static constexpr std::string_view kKey = "transform";
		static constexpr bool kReflect = true;

		Vector3 Position    = Vector3::Zero;
		Quaternion Rotation = Quaternion::Identity;
		Vector3 Scale       = Vector3::One;

		[[nodiscard]] Matrix ToMatrix() const;
	};
}
