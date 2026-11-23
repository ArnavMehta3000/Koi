#include "Koi/Modules/Physics/Source/PhysicsSystem.h"
#include "Koi/Modules/Physics/Source/PhysicsComponents.h"
#include "Koi/Modules/Physics/Source/PhysicsEvents.h"
#include "Koi/Modules/Physics/Source/Box3DConvert.h"
#include "Koi/Modules/Physics/Source/ScaledCollider.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Modules/Physics/Source/PhysicsModule.h"
#include <box3d/box3d.h>

namespace koi
{
	namespace
	{
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
			return ::b3Body_IsValid(id) && internal::UserDataToEntity(::b3Body_GetUserData(id)) == e.GetId();
		}

		bool OwnsShape(b3ShapeId id, Entity e)
		{
			return ::b3Shape_IsValid(id) && internal::UserDataToEntity(::b3Shape_GetUserData(id)) == e.GetId();
		}

		// Null when the shape was destroyed or its entity is gone
		Entity EntityOf(ECS& ecs, b3ShapeId shape)
		{
			if (!::b3Shape_IsValid(shape)) return Entity{};

			const Entity e = ecs.FromId(internal::UserDataToEntity(::b3Shape_GetUserData(shape)));
			return e.IsAlive() ? e : Entity{};
		}

		// Sent to both sides, so either one can observe it
		template<typename Event>
		void EmitPair(World& world, b3ShapeId sensor, b3ShapeId visitor)
		{
			const Entity a = EntityOf(world.GetECS(), sensor);
			const Entity b = EntityOf(world.GetECS(), visitor);

			if (a) world.Emit(a, Event{ .Other = b });
			if (b) world.Emit(b, Event{ .Other = a });
		}

		// No RigidBody means a static body
		b3BodyId CreateBody(b3WorldId world, Entity e, const Transform& xform, const RigidBody* body)
		{
			b3BodyDef def = ::b3DefaultBodyDef();
			def.position  = internal::ToB3Pos(xform.Position);
			def.rotation  = internal::ToB3(xform.Rotation);
			def.userData  = internal::EntityToUserData(e.GetId());
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
			def.userData                 = internal::EntityToUserData(e.GetId());
			def.density                  = collider.Density;
			def.baseMaterial.friction    = collider.Friction;
			def.baseMaterial.restitution = collider.Restitution;
			def.isSensor                 = collider.IsSensor;
			def.enableSensorEvents       = collider.SensorEvents;
			def.enableContactEvents      = collider.ContactEvents;

			const internal::ScaledCollider shape = internal::ScaleCollider(collider, xform.Scale);

			switch (collider.Shape)
			{
			case ColliderShape::Box:
			{
				const Vector3& half = shape.HalfExtents;
				const b3BoxHull box = ::b3MakeOffsetBoxHull(half.x, half.y, half.z, internal::ToB3(shape.Center));
				return ::b3CreateHullShape(body, &def, &box.base);
			}
			case ColliderShape::Sphere:
			{
				const b3Sphere sphere{ .center = internal::ToB3(shape.Center), .radius = shape.Radius };
				return ::b3CreateSphereShape(body, &def, &sphere);
			}
			case ColliderShape::Capsule:
			{
				const Vector3 axis(0.0f, shape.HalfHeight, 0.0f);
				const b3Capsule capsule
				{
					.center1 = internal::ToB3(shape.Center - axis),
					.center2 = internal::ToB3(shape.Center + axis),
					.radius  = shape.Radius,
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

	void PhysicsSystem::Build(World& world)
	{
		TaskSystem::Build(world);

		ECS& ecs = world.GetECS();

		// Destroy b3Body when removing RigidBody component
		ecs.OnRemove<RigidBody>([](Entity e, RigidBody& body)
		{
			if (OwnsBody(body.BodyId.get(), e))
			{
				::b3DestroyBody(body.BodyId.get());
			}
		});

		// When a Collider is removed, destroy shape/body
		ecs.OnRemove<Collider>([](Entity e, Collider& collider)
		{
			const b3ShapeId shape = collider.ShapeId.get();
			if (!OwnsShape(shape, e)) return;

			// A lone Collider's static body goes with it; a RigidBody keeps its body and only loses the shape
			const b3BodyId body = ::b3Shape_GetBody(shape);
			const RigidBody* rigidBody = e.TryGet<const RigidBody>();
			if (rigidBody && B3_ID_EQUALS(rigidBody->BodyId.get(), body))
			{
				::b3DestroyShape(shape, true);
			}
			else
			{
				::b3DestroyBody(body);
			}
		});
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
		PushKinematics(ctx.World, ctx.DeltaTime);

		{
			KOI_SCOPE("Box3D Step");
			::b3World_Step(m_b3WorldId, ctx.DeltaTime, settings.SubSteps);
		}

		PullDynamics(ctx.World);
		EmitSensorEvents(ctx.World);
	}

	void PhysicsSystem::OnBeginPlay(World& world)
	{
		ZoneScoped;
		CreatePhysicsWorld(PhysicsSettings::Resolve(world));

		KOI_LOG(Physics, Info, "PhysicsSystem::OnBeginPlay");
	}

	void PhysicsSystem::OnEndPlay([[maybe_unused]] World& world)
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
		ZoneScoped;
		if (!::b3World_IsValid(m_b3WorldId)) { return; }

		::b3DestroyWorld(m_b3WorldId);
		m_b3WorldId = b3_nullWorldId;
	}

	void PhysicsSystem::SetupBodies(World& world)
	{
		ZoneScoped;
		ECS& ecs = world.GetECS();
		i64 created = 0;
		i64 rebuilt = 0;

		// Rigid body first so that a collider find its partner RB

		ecs.Each<const WorldTransform, RigidBody>([&](Entity e, const WorldTransform& pose, RigidBody& body)
		{
			if (OwnsBody(body.BodyId.get(), e)) return;
			body.BodyId = CreateBody(m_b3WorldId, e, pose.AsTransform(), &body);
			++created;
		});

		ecs.Each<const WorldTransform, Collider>([&](Entity e, const WorldTransform& pose, Collider& collider)
		{
			const Transform xform = pose.AsTransform();
			const b3ShapeId current = collider.ShapeId.get();
			if (OwnsShape(current, e))
			{
				if (collider.BuiltScale.get() == xform.Scale) return;

				// Box3D has no scale, so a rescaled collider gets a new shape on the same body
				const b3BodyId bodyId = ::b3Shape_GetBody(current);
				::b3DestroyShape(current, true);
				collider.ShapeId    = CreateShape(bodyId, e, xform, collider);
				collider.BuiltScale = xform.Scale;
				++rebuilt;
				return;
			}

			const RigidBody* body = e.TryGet<const RigidBody>();
			const b3BodyId bodyId = body ? body->BodyId.get() : CreateBody(m_b3WorldId, e, xform, nullptr);
			collider.ShapeId    = CreateShape(bodyId, e, xform, collider);
			collider.BuiltScale = xform.Scale;
			++created;
		});

		if (created > 0) FrameStats::Count("Physics Created", created);
		if (rebuilt > 0) FrameStats::Count("Physics Rescaled", rebuilt);
	}

	// Game code owns a kinematic Transform; a target transform gives the body a velocity, so contacts stay correct
	void PhysicsSystem::PushKinematics(World& world, f32 dt)
	{
		ZoneScoped;
		world.GetECS().Each<const WorldTransform, const RigidBody>([dt](Entity e, const WorldTransform& xform, const RigidBody& body)
		{
			if (body.Type != BodyType::Kinematic || !OwnsBody(body.BodyId.get(), e)) return;
			::b3Body_SetTargetTransform(body.BodyId.get(), internal::ToB3(xform.AsTransform()), dt, true);
		});
	}

	// Physics owns a dynamic Transform. Only bodies that moved this step are reported; scale stays the entity's
	void PhysicsSystem::PullDynamics(World& world)
	{
		ZoneScoped;
		ECS& ecs = world.GetECS();
		const b3BodyEvents events = ::b3World_GetBodyEvents(m_b3WorldId);
		if (events.moveCount > 0) FrameStats::Count("Physics Moved", events.moveCount);

		for (i32 i = 0; i < events.moveCount; ++i)
		{
			const b3BodyMoveEvent& move = events.moveEvents[i];

			const Entity e = ecs.FromId(internal::UserDataToEntity(move.userData));
			if (!e.IsAlive()) continue;

			const RigidBody* body = e.TryGet<const RigidBody>();
			if (!body || body->Type != BodyType::Dynamic) continue;

			Transform* xform = e.TryGet<Transform>();
			if (!xform) continue;

			const Vector3    position = internal::ToKoi(move.transform.p);
			const Quaternion rotation = internal::ToKoi(move.transform.q);
			const Entity parent = e.GetParent();
			if (const WorldTransform* parentWorld = parent ? parent.TryGet<const WorldTransform>() : nullptr)
			{
				const Transform local = WorldTransform::ToLocal(Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position), parentWorld);
				xform->Position = local.Position;
				xform->Rotation = local.Rotation;
			}
			else
			{
				xform->Position = position;
				xform->Rotation = rotation;
			}
		}
	}

	void PhysicsSystem::EmitSensorEvents(World& world)
	{
		ZoneScoped;
		const b3SensorEvents events = ::b3World_GetSensorEvents(m_b3WorldId);
		if (const i64 count = events.beginCount + events.endCount; count > 0) FrameStats::Count("Sensor Events", count);

		for (i32 i = 0; i < events.beginCount; ++i)
		{
			EmitPair<SensorBegin>(world, events.beginEvents[i].sensorShapeId, events.beginEvents[i].visitorShapeId);
		}

		for (i32 i = 0; i < events.endCount; ++i)
		{
			EmitPair<SensorEnd>(world, events.endEvents[i].sensorShapeId, events.endEvents[i].visitorShapeId);
		}
	}
}
