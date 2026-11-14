#include "HeadlessEngine.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <doctest/doctest.h>
#include <toml++/toml.hpp>

using namespace koi;

namespace
{
	constexpr std::string_view kScene = R"(
		[[entity]]
		name = "Crate"

			[entity.transform]
			position = [ 1.0, 2.0, 3.0 ]
			rotation = [ 0.0, 0.0, 0.0, 1.0 ]
			scale    = [ 2.0, 2.0, 2.0 ]

			[entity.mesh]
			path = "engine://Content/Meshes/Box.kmesh"

			[entity.material]
			path = "engine://Content/Materials/Default.kmat"

		[[entity]]
		name = "Sun"

			[entity.light]
			type          = "Directional"
			intensity     = 3.0
			casts_shadows = true
	)";

	const toml::table* FindEntity(const toml::table& scene, std::string_view name)
	{
		for (const toml::node& node : *scene["entity"].as_array())
		{
			const toml::table* entity = node.as_table();
			if (entity && (*entity)["name"].value_or(std::string_view()) == name) return entity;
		}
		return nullptr;
	}
}

TEST_CASE("Scene save then load gives back the same scene")
{
	test::HeadlessEngine();

	World world;
	REQUIRE(world.LoadFromTable(toml::parse(kScene)));
	const toml::table saved = world.SaveToTable();

	World reloaded;
	REQUIRE(reloaded.LoadFromTable(saved));
	CHECK(reloaded.SaveToTable() == saved);
}

TEST_CASE("Saved scenes name assets by id and keep component values")
{
	test::HeadlessEngine();

	World world;
	REQUIRE(world.LoadFromTable(toml::parse(kScene)));
	const toml::table saved = world.SaveToTable();

	// Crate, Sun, and the camera every scene gets when it has none
	CHECK(saved["entity"].as_array()->size() == 3);
	CHECK(FindEntity(saved, "Camera"));

	const toml::table* crate = FindEntity(saved, "Crate");
	REQUIRE(crate);
	CHECK((*crate)["mesh"]["id"].value_or(std::string()) == utils::ToString(utils::UUIDFromName("Koi.Engine.Mesh.Box")));
	CHECK_FALSE((*crate)["mesh"]["path"]);
	CHECK((*crate)["transform"]["scale"][0].value_or(0.0) == 2.0);

	const toml::table* sun = FindEntity(saved, "Sun");
	REQUIRE(sun);
	CHECK((*sun)["light"]["type"].value_or(std::string()) == "Directional");
	CHECK((*sun)["light"]["casts_shadows"].value_or(false));
	CHECK((*sun)["transform"]);  // required, so added even though the file left it out
}

TEST_CASE("Spawned entities get their assets and save with the world")
{
	test::HeadlessEngine();

	World world;
	const Entity crate = world.Spawn("Spawned").WithMesh("engine://Content/Meshes/Sphere.kmesh").At(Vector3(0.0f, 5.0f, 0.0f));

	const MeshRef* mesh = crate.try_get<MeshRef>();
	REQUIRE(mesh);
	CHECK(world.GetMesh(mesh->Handle));
	CHECK(crate.get<Transform>().Position.y == 5.0f);

	CHECK(FindEntity(world.SaveToTable(), "Spawned"));
}

TEST_CASE("Clearing a world releases the assets only it used")
{
	const AssetKey torus("engine://Content/Meshes/Torus.kmesh");
	AssetCache<GPUMesh>& meshes = test::HeadlessEngine().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();

	World world;
	world.Spawn("A").WithMesh(torus.Path());
	world.Spawn("B").WithMesh(torus.Path());
	CHECK(meshes.IsResident(torus));

	world.Clear();
	CHECK(meshes.GetRefCount(torus) == 0);

	meshes.EvictUnused(test::HeadlessEngine().GetSubsystem<Renderer>().GetDevice());
	CHECK_FALSE(meshes.IsResident(torus));
}
