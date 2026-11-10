#include "Koi/Engine/Physics/PhysicsSystem.h"

namespace koi
{
	namespace
	{
		constexpr b3Vec3 DEFAULT_GRAVITY{ 0.0f, -9.81f, 0.0f };
	}

	PhysicsSystem::~PhysicsSystem()
	{
		// Somehow we have not destroyed the world, do it now
		DestroyPhysicsWorld();
	}

	void PhysicsSystem::Run(const SystemContext& ctx)
	{
		// The world can be invalid when not playing, yet this function will be called every Tick
		// So gaurd the running/stepping of the b3World
		if (!b3World_IsValid(m_b3WorldId)) { return; }

		constexpr i32 SUB_STEP_COUNT = 4;
		::b3World_Step(m_b3WorldId, ctx.DeltaTime, SUB_STEP_COUNT);
	}

	void PhysicsSystem::OnBeginPlay(World& world)
	{
		CreatePhysicsWorld();
	}

	void PhysicsSystem::OnEndPlay(World& world)
	{
		DestroyPhysicsWorld();
	}

	void PhysicsSystem::CreatePhysicsWorld()
	{
		b3WorldDef worldDef = ::b3DefaultWorldDef();
		worldDef.gravity = DEFAULT_GRAVITY;
		m_b3WorldId = ::b3CreateWorld(&worldDef);
	}

	void PhysicsSystem::DestroyPhysicsWorld()
	{
		if (!b3World_IsValid(m_b3WorldId)) { return; }

		::b3DestroyWorld(m_b3WorldId);
		m_b3WorldId = b3_nullWorldId;
	}
}
