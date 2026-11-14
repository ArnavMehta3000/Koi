#pragma once
#include "Koi/Engine/ECS/System.h"

namespace koi
{
	// Draws every live collider where Box3D has it
	class PhysicsDebugDrawSystem : public TaskSystem
	{
	public:
		[[nodiscard]] std::string_view Name() const override { return "PhysicsDebugDraw"; }
		[[nodiscard]] SystemPhase Phase() const override { return SystemPhase::PreRender; }

		void Run(const SystemContext& ctx) override;
	};
}
