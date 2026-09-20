#pragma once
#include "Koi/Engine/Math/Math.h"
#include <array>

namespace game
{
	using Lanes = std::array<koi::Vector3, 3>;

	// Drag fields plus debug spheres; active highlights one lane, -1 none
	void DrawLanes(Lanes& positions, koi::i32 active);
}
