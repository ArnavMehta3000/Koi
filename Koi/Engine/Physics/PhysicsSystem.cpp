#include "Koi/Engine/Physics/PhysicsSystem.h"

namespace koi
{
	void PhysicsSystem::Run(const SystemContext& ctx)
	{
		// The world can be invalid when not playing, yet this function will be called every Tick
		// So gaurd the running/stepping of the b3World
		if (!m_worldValid) { return; }

		constexpr i32 SUB_STEP_COUNT = 4;
		b3World_Step(m_b3WorldId, ctx.DeltaTime, SUB_STEP_COUNT);
	}

	void PhysicsSystem::OnBeginPlay(World& world)
	{
		static b3WorldDef worldDef = ::b3DefaultWorldDef();
		m_b3WorldId = ::b3CreateWorld(&worldDef);
		m_worldValid = true;
	}

	void PhysicsSystem::OnEndPlay(World& world)
	{
		::b3DestroyWorld(m_b3WorldId);
		m_b3WorldId = b3_nullWorldId;
		m_worldValid = false;
	}
}
