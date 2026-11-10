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
