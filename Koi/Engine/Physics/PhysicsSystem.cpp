#include "Koi/Engine/Physics/PhysicsSystem.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include <box3d/box3d.h>

namespace koi
{
	PhysicsSystem::~PhysicsSystem()
	{
		// Covers a World destroyed mid-play, which never sees EndPlay
		DestroyPhysicsWorld();
	}

	void PhysicsSystem::Run(const SystemContext& ctx)
	{
		if (!::b3World_IsValid(m_b3WorldId)) { return; }

		const PhysicsSettings& settings = PhysicsSettings::Resolve(ctx.World);
		::b3World_Step(m_b3WorldId, ctx.DeltaTime, settings.SubSteps);
	}

	void PhysicsSystem::OnBeginPlay(World& world)
	{
		CreatePhysicsWorld(PhysicsSettings::Resolve(world));
	}

	void PhysicsSystem::OnEndPlay(World& /*world*/)
	{
		DestroyPhysicsWorld();
	}

	void PhysicsSystem::CreatePhysicsWorld(const PhysicsSettings& settings)
	{
		b3WorldDef worldDef = ::b3DefaultWorldDef();
		worldDef.gravity = { settings.Gravity.x, settings.Gravity.y, settings.Gravity.z };
		m_b3WorldId = ::b3CreateWorld(&worldDef);
	}

	void PhysicsSystem::DestroyPhysicsWorld()
	{
		if (!::b3World_IsValid(m_b3WorldId)) { return; }

		::b3DestroyWorld(m_b3WorldId);
		m_b3WorldId = b3_nullWorldId;
	}
}
