#pragma once
#include "Koi/Engine/Math/MathTypes.h"
#include <array>

namespace game
{
	using Lanes = std::array<koi::Vector3, 3>;

	// Debug spheres at each lane
	void DrawLaneSpheres(const Lanes& positions, koi::i32 active);
}
