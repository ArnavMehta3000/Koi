#include "Koi/Engine/Physics/PhysicsSystem.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/Physics/Box3DConvert.h"
#include "Koi/Engine/Logging/LogCategories.h"
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

		const b3Vec3 b3Grav = ::b3World_GetGravity(m_b3WorldId);
		if (internal::ToKoi(b3Grav) != settings.Gravity)
		{
			// User updated component gravity, update world gravity
			::b3World_SetGravity(m_b3WorldId, internal::ToB3(settings.Gravity));
		}

		::b3World_Step(m_b3WorldId, ctx.DeltaTime, settings.SubSteps);
	}

	void PhysicsSystem::OnBeginPlay(World& world)
	{
		CreatePhysicsWorld(PhysicsSettings::Resolve(world));

		KOI_LOG(Physics, Info, "PhysicsSystem::OnBeginPlay");
	}

	void PhysicsSystem::OnEndPlay(World& world)
	{
		DestroyPhysicsWorld();
		KOI_LOG(Physics, Info, "PhysicsSystem::OnEndPlay");
	}

	void PhysicsSystem::CreatePhysicsWorld(const PhysicsSettings& settings)
	{
		b3WorldDef worldDef = ::b3DefaultWorldDef();
		worldDef.gravity = internal::ToB3(settings.Gravity);

		m_b3WorldId = ::b3CreateWorld(&worldDef);
	}

	void PhysicsSystem::DestroyPhysicsWorld()
	{
		if (!::b3World_IsValid(m_b3WorldId)) { return; }

		::b3DestroyWorld(m_b3WorldId);
		m_b3WorldId = b3_nullWorldId;
	}
}
