#include "Components/Lanes.h"
#include "Koi/Engine/Graphics/DebugDraw.h"

namespace game
{
	void DrawLaneSpheres(const Lanes& positions, koi::i32 active)
	{
		static const koi::Color activeCol(1.0f, 0.8f, 0.2f, 1.0f);
		static const koi::Color inactiveCol(0.3f, 0.9f, 1.0f, 1.0f);
		for (size_t i = 0; i < positions.size(); ++i)
		{
			koi::DebugDraw::Sphere(positions[i], 0.25f, static_cast<koi::i32>(i) == active ? activeCol : inactiveCol);
		}
	}
}
