#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Core/UUID.h"
#include <doctest/doctest.h>
#include <string>

using namespace koi;

TEST_CASE("UUIDFromName is stable and distinct per name")
{
	CHECK(utils::UUIDFromName("Koi.Engine.Mesh.Box") == utils::UUIDFromName("Koi.Engine.Mesh.Box"));
	CHECK(utils::UUIDFromName("Koi.Engine.Mesh.Box") != utils::UUIDFromName("Koi.Engine.Mesh.Sphere"));
	CHECK_FALSE(utils::UUIDFromName("Koi.Engine.Mesh.Box").is_nil());
}

TEST_CASE("UUID string round-trip")
{
	const UUID id = utils::NewUUID();
	CHECK(utils::UUIDFromString(utils::ToString(id)) == id);
	CHECK(utils::UUIDFromString("not a uuid").is_nil());
}

TEST_CASE("HandlePool rejects stale handles")
{
	HandlePool<std::string> pool;
	const auto a = pool.Emplace("a");
	REQUIRE(pool.Get(a));
	CHECK(*pool.Get(a) == "a");

	pool.Erase(a);
	CHECK_FALSE(pool.IsValid(a));

	// The recycled slot gets a new generation, so the old handle stays dead
	const auto b = pool.Emplace("b");
	CHECK(b.GetIndex() == a.GetIndex());
	CHECK_FALSE(pool.IsValid(a));
	CHECK(*pool.Get(b) == "b");

	pool.Clear();
	CHECK_FALSE(pool.IsValid(b));
}
