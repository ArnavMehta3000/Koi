#pragma once
#include "Koi/Engine/ECS/System.h"

namespace koi
{
	// Draws every live collider where Box3D has it
	class PhysicsDebugDrawSystem : public TaskSystem
	{
	public:
		using Phase = PreRender;

		void Run(const SystemContext& ctx) override;
	};
}
