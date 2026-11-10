#include "Koi/Engine/Physics/PhysicsSystem.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/Physics/Box3DConvert.h"
#include "Koi/Engine/Logging/LogCategories.h"
#include <box3d/box3d.h>
#include <algorithm>
#include <cmath>

namespace koi
{
	namespace
	{
		constexpr f32 kMinShapeExtent = 0.005f;  // Box3D asserts on degenerate shapes, e.g. a zero scale

		b3BodyType ToB3BodyType(BodyType type)
		{
			switch (type)
			{
			case BodyType::Static:    return b3_staticBody;
			case BodyType::Kinematic: return b3_kinematicBody;
			case BodyType::Dynamic:   return b3_dynamicBody;
			}
			return b3_staticBody;
		}

		// A duplicated entity copies its source's ids, so an id only counts if it points back at this entity
		bool OwnsBody(b3BodyId id, Entity e)
		{
			return ::b3Body_IsValid(id) && internal::UserDataToEntity(::b3Body_GetUserData(id)) == e.id();
		}

		bool OwnsShape(b3ShapeId id, Entity e)
		{
			return ::b3Shape_IsValid(id) && internal::UserDataToEntity(::b3Shape_GetUserData(id)) == e.id();
		}

		// No RigidBody means a static body
		b3BodyId CreateBody(b3WorldId world, Entity e, const Transform& xform, const RigidBody* body)
		{
			b3BodyDef def = ::b3DefaultBodyDef();
			def.position  = internal::ToB3Pos(xform.Position);
			def.rotation  = internal::ToB3(xform.Rotation);
			def.userData  = internal::EntityToUserData(e.id());
			def.type      = b3_staticBody;

			if (body)
			{
				def.type           = ToB3BodyType(body->Type);
				def.gravityScale   = body->GravityScale;
				def.linearDamping  = body->LinearDamping;
				def.angularDamping = body->AngularDamping;
				def.motionLocks    = {
					body->LockPosition[0], body->LockPosition[1], body->LockPosition[2],
					body->LockRotation[0], body->LockRotation[1], body->LockRotation[2],
				};
			}

			return ::b3CreateBody(world, &def);
		}

		b3ShapeId CreateShape(b3BodyId body, Entity e, const Transform& xform, const Collider& collider)
		{
			b3ShapeDef def = ::b3DefaultShapeDef();
			def.userData                 = internal::EntityToUserData(e.id());
			def.density                  = collider.Density;
			def.baseMaterial.friction    = collider.Friction;
			def.baseMaterial.restitution = collider.Restitution;
			def.isSensor                 = collider.IsSensor;
			def.enableSensorEvents       = collider.SensorEvents;
			def.enableContactEvents      = collider.ContactEvents;

			const Vector3 scale(std::abs(xform.Scale.x), std::abs(xform.Scale.y), std::abs(xform.Scale.z));
			const Vector3 center = collider.Offset * scale;

			switch (collider.Shape)
			{
			case ColliderShape::Box:
			{
				const Vector3 half = collider.HalfExtents * scale;
				const b3BoxHull box = ::b3MakeOffsetBoxHull(
					std::max(half.x, kMinShapeExtent), std::max(half.y, kMinShapeExtent), std::max(half.z, kMinShapeExtent),
					internal::ToB3(center));
				return ::b3CreateHullShape(body, &def, &box.base);
			}
			case ColliderShape::Sphere:
			{
				const f32 radius = collider.Radius * std::max({ scale.x, scale.y, scale.z });
				const b3Sphere sphere{ .center = internal::ToB3(center), .radius = std::max(radius, kMinShapeExtent) };
				return ::b3CreateSphereShape(body, &def, &sphere);
			}
			case ColliderShape::Capsule:
			{
				const Vector3 axis(0.0f, collider.HalfHeight * scale.y, 0.0f);
				const f32 radius = collider.Radius * std::max(scale.x, scale.z);
				const b3Capsule capsule
				{
					.center1 = internal::ToB3(center - axis),
					.center2 = internal::ToB3(center + axis),
					.radius  = std::max(radius, kMinShapeExtent),
				};
				return ::b3CreateCapsuleShape(body, &def, &capsule);
			}
			}
			return b3_nullShapeId;
		}
	}

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

		SetupBodies(ctx.World);

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

	void PhysicsSystem::SetupBodies(World& world)
	{
		ECS& ecs = world.GetECS();

		// Rigid body first so that a collider find its partner RB

		ecs.query<const Transform, RigidBody>().each([this](Entity e, const Transform& xform, RigidBody& body)
		{
			if (OwnsBody(body.BodyId.get(), e)) return;
			body.BodyId = CreateBody(m_b3WorldId, e, xform, &body);
		});

		ecs.query<const Transform, Collider>().each([this](Entity e, const Transform& xform, Collider& collider)
		{
			if (OwnsShape(collider.ShapeId.get(), e)) return;

			const RigidBody* body = e.try_get<RigidBody>();
			const b3BodyId bodyId = body ? body->BodyId.get() : CreateBody(m_b3WorldId, e, xform, nullptr);
			collider.ShapeId = CreateShape(bodyId, e, xform, collider);
		});
	}
}
