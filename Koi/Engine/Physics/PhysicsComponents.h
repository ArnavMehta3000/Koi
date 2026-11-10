#pragma once
#include "Koi/Engine/Math/MathTypes.h"
#include <string_view>

namespace koi
{
	class World;

	struct PhysicsSettings
	{
		static constexpr std::string_view kKey = "physics_settings";
		static constexpr bool kReflect = true;

		Vector3 Gravity  = Vector3(0.0f, -9.81f, 0.0f);
		i32     SubSteps = 4;

		[[nodiscard]] static const PhysicsSettings& Resolve(const World& world);
	};
}
