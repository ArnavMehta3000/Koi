#include "HeadlessEngine.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/Physics/PhysicsSystem.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <box3d/box3d.h>
#include <doctest/doctest.h>
#include <optional>

using namespace koi;

namespace
{
	// The registry is process-wide; AppBase isn't running, so register it the way HostInit does
	void RegisterPhysicsSystem()
	{
		static const bool s_registered = []
		{
			SystemRegistry::Get().Register<PhysicsSystem>();
			return true;
		}();
		(void)s_registered;
	}

	struct PlayingWorld
	{
		PlayingWorld()
		{
			test::HeadlessEngine().SetPlayState(PlayState::Playing);
			RegisterPhysicsSystem();
			World.emplace();
		}

		~PlayingWorld() { test::HeadlessEngine().SetPlayState(PlayState::Edit); }

		std::optional<koi::World> World;
	};
}

// Only proves the box3d package builds and links in this mode
TEST_CASE("Box3D drops a sphere under gravity")
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity    = { 0.0f, -10.0f, 0.0f };
	const b3WorldId world = b3CreateWorld(&worldDef);

	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type      = b3_dynamicBody;
	bodyDef.position  = { 0.0f, 10.0f, 0.0f };
	const b3BodyId body = b3CreateBody(world, &bodyDef);

	const b3ShapeDef shapeDef = b3DefaultShapeDef();
	const b3Sphere sphere     = { .center = { 0.0f, 0.0f, 0.0f }, .radius = 0.5f };
	(void)b3CreateSphereShape(body, &shapeDef, &sphere);

	for (int i = 0; i < 60; ++i)
	{
		b3World_Step(world, 1.0f / 60.0f, 4);
	}

	// One second of free fall from 10 m lands near 5 m
	CHECK(b3Body_GetPosition(body).y == doctest::Approx(5.0f).epsilon(0.05));

	b3DestroyWorld(world);
}

TEST_CASE("A world without PhysicsSettings resolves to the defaults")
{
	PlayingWorld w;

	const PhysicsSettings& settings = PhysicsSettings::Resolve(*w.World);
	CHECK(settings.Gravity.y == doctest::Approx(-9.81f));
	CHECK(settings.SubSteps == 4);
}

TEST_CASE("A PhysicsSettings component in the world overrides the defaults")
{
	PlayingWorld w;
	(void)w.World->Spawn("Physics").With(PhysicsSettings{ .Gravity = Vector3(0.0f, -1.62f, 0.0f), .SubSteps = 8 });

	const PhysicsSettings& settings = PhysicsSettings::Resolve(*w.World);
	CHECK(settings.Gravity.y == doctest::Approx(-1.62f));
	CHECK(settings.SubSteps == 8);
}

TEST_CASE("PhysicsSettings saves snake_case keys and fills missing ones")
{
	const PhysicsSettings in{ .Gravity = Vector3(1.0f, 2.0f, 3.0f), .SubSteps = 6 };
	const toml::table saved = Serializer<PhysicsSettings>::Serialize(in);
	CHECK(saved.contains("gravity"));
	CHECK(saved.contains("sub_steps"));

	const std::optional<PhysicsSettings> loaded = Serializer<PhysicsSettings>::Deserialize(saved);
	REQUIRE(loaded);
	CHECK(loaded->Gravity == in.Gravity);
	CHECK(loaded->SubSteps == 6);

	const toml::table partial{ { "sub_steps", 2 } };
	const std::optional<PhysicsSettings> filled = Serializer<PhysicsSettings>::Deserialize(partial);
	REQUIRE(filled);
	CHECK(filled->Gravity.y == doctest::Approx(-9.81f));
	CHECK(filled->SubSteps == 2);
}

TEST_CASE("RigidBody saves its settings but never its runtime body")
{
	RigidBody in{ .Type = BodyType::Kinematic, .GravityScale = 0.5f, .LockRotation = { true, false, true } };
	in.BodyId = b3BodyId{ .index1 = 7, .world0 = 0, .generation = 1 };

	const toml::table saved = Serializer<RigidBody>::Serialize(in);
	CHECK(saved.contains("gravity_scale"));
	CHECK(saved.contains("lock_rotation"));
	CHECK_FALSE(saved.contains("body_id"));

	const std::optional<RigidBody> out = Serializer<RigidBody>::Deserialize(saved);
	REQUIRE(out);
	CHECK(out->Type == BodyType::Kinematic);
	CHECK(out->GravityScale == doctest::Approx(0.5f));
	CHECK(out->LockRotation == std::array{ true, false, true });
	CHECK(B3_IS_NULL(out->BodyId.get()));
}

TEST_CASE("Collider round-trips its shape and fills missing keys with the defaults")
{
	const Collider in{ .Shape = ColliderShape::Capsule, .Radius = 0.25f, .HalfHeight = 1.0f, .IsSensor = true, .SensorEvents = false };
	const toml::table saved = Serializer<Collider>::Serialize(in);
	CHECK(saved.contains("half_height"));
	CHECK(saved.contains("is_sensor"));
	CHECK_FALSE(saved.contains("shape_id"));

	const std::optional<Collider> out = Serializer<Collider>::Deserialize(saved);
	REQUIRE(out);
	CHECK(out->Shape == ColliderShape::Capsule);
	CHECK(out->Radius == doctest::Approx(0.25f));
	CHECK(out->HalfHeight == doctest::Approx(1.0f));
	CHECK(out->IsSensor);
	CHECK_FALSE(out->SensorEvents);

	const toml::table partial{ { "shape", "Sphere" } };
	const std::optional<Collider> filled = Serializer<Collider>::Deserialize(partial);
	REQUIRE(filled);
	CHECK(filled->Shape == ColliderShape::Sphere);
	CHECK(filled->HalfExtents == Vector3(0.5f, 0.5f, 0.5f));
	CHECK(filled->SensorEvents);
	CHECK_FALSE(filled->ContactEvents);
}

// Box3D asserts on a stale or null world id, so any unguarded path aborts the run
TEST_CASE("Play and Stop pair up safely, however often they come")
{
	PlayingWorld w;

	w.World->EndPlay();  // Stop with no Play before it

	for (int session = 0; session < 3; ++session)
	{
		w.World->BeginPlay();
		CHECK_NOTHROW(w.World->Update(3.0f * World::kFixedStep));
		w.World->EndPlay();
	}

	CHECK_NOTHROW(w.World->Update(2.0f * World::kFixedStep));  // stepping with no b3World

	w.World->BeginPlay();
	w.World.reset();  // destroyed mid-play: the system's destructor cleans up
}

namespace
{
	u64 EntityOf(b3BodyId body) { return static_cast<u64>(reinterpret_cast<uintptr_t>(b3Body_GetUserData(body))); }
}

TEST_CASE("The first fixed step gives a RigidBody and its Collider a body that points back at the entity")
{
	PlayingWorld w;
	w.World->BeginPlay();
	const Entity crate = w.World->Spawn().With(RigidBody{ .Type = BodyType::Kinematic }).With(Collider{}).Build();

	w.World->Update(World::kFixedStep);

	const b3BodyId body   = crate.get<RigidBody>().BodyId.get();
	const b3ShapeId shape = crate.get<Collider>().ShapeId.get();
	REQUIRE(b3Body_IsValid(body));
	REQUIRE(b3Shape_IsValid(shape));
	CHECK(b3Body_GetType(body) == b3_kinematicBody);
	CHECK(B3_ID_EQUALS(b3Shape_GetBody(shape), body));
	CHECK(EntityOf(body) == crate.id());
	CHECK(b3Shape_GetType(shape) == b3_hullShape);

	w.World->EndPlay();
}

TEST_CASE("A Collider without a RigidBody gets a static body")
{
	PlayingWorld w;
	w.World->BeginPlay();
	const Entity wall = w.World->Spawn().With(Collider{ .IsSensor = true }).Build();

	w.World->Update(World::kFixedStep);

	const b3ShapeId shape = wall.get<Collider>().ShapeId.get();
	REQUIRE(b3Shape_IsValid(shape));
	CHECK(b3Body_GetType(b3Shape_GetBody(shape)) == b3_staticBody);
	CHECK(b3Shape_IsSensor(shape));

	w.World->EndPlay();
}

TEST_CASE("Shapes bake in the entity's scale")
{
	PlayingWorld w;
	w.World->BeginPlay();
	const Entity ball = w.World->Spawn().With(Collider{ .Shape = ColliderShape::Sphere, .Radius = 0.5f, .Offset = Vector3(1.0f, 0.0f, 0.0f) }).Build();
	ball.set<Transform>({ .Scale = Vector3(2.0f, 3.0f, -1.0f) });
	const Entity flat = w.World->Spawn().With(Collider{}).Build();
	flat.set<Transform>({ .Scale = Vector3(1.0f, 0.0f, 1.0f) });

	w.World->Update(World::kFixedStep);

	const b3Sphere sphere = b3Shape_GetSphere(ball.get<Collider>().ShapeId.get());
	CHECK(sphere.radius == doctest::Approx(1.5f));
	CHECK(sphere.center.x == doctest::Approx(2.0f));
	CHECK(b3Shape_IsValid(flat.get<Collider>().ShapeId.get()));  // zero scale is clamped, not an assert

	w.World->EndPlay();
}

TEST_CASE("A duplicated entity gets its own body instead of sharing its source's")
{
	PlayingWorld w;
	w.World->BeginPlay();
	const Entity original = w.World->Spawn().With(RigidBody{}).With(Collider{}).Build();
	w.World->Update(World::kFixedStep);

	const Entity copy = w.World->Spawn().With(RigidBody(original.get<RigidBody>())).With(Collider(original.get<Collider>())).Build();
	w.World->Update(World::kFixedStep);

	const b3BodyId originalBody = original.get<RigidBody>().BodyId.get();
	const b3BodyId copyBody     = copy.get<RigidBody>().BodyId.get();
	REQUIRE(b3Body_IsValid(copyBody));
	CHECK_FALSE(B3_ID_EQUALS(originalBody, copyBody));
	CHECK(EntityOf(originalBody) == original.id());
	CHECK(EntityOf(copyBody) == copy.id());
	CHECK(B3_ID_EQUALS(b3Shape_GetBody(copy.get<Collider>().ShapeId.get()), copyBody));
	CHECK(b3Body_GetShapeCount(originalBody) == 1);

	w.World->EndPlay();
}

TEST_CASE("A dynamic body falls under the scene's PhysicsSettings gravity")
{
	PlayingWorld w;
	(void)w.World->Spawn("Physics").With(PhysicsSettings{ .Gravity = Vector3(0.0f, -2.0f, 0.0f) });
	const Entity rock = w.World->Spawn().With(RigidBody{}).With(Collider{ .Shape = ColliderShape::Sphere }).At(Vector3(0.0f, 10.0f, 0.0f)).Build();
	w.World->BeginPlay();

	for (int step = 0; step < 60; ++step)
	{
		w.World->Update(World::kFixedStep);
	}

	// Created and stepped in the same Run, so one second of fall: 0.5 * 2 * 1^2
	const f32 fallen = 10.0f - b3Body_GetPosition(rock.get<RigidBody>().BodyId.get()).y;
	CHECK(fallen == doctest::Approx(1.0f).epsilon(0.05));

	w.World->EndPlay();
}
