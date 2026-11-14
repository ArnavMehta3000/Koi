#include "HeadlessEngine.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <doctest/doctest.h>

using namespace koi;

namespace
{
	constexpr f64 kGrace = 5.0;

	struct MeshCache
	{
		MeshCache()
			: Cache(test::HeadlessEngine().GetSubsystem<AssetRegistry>().Cache<GPUMesh>())
			, Device(test::HeadlessEngine().GetSubsystem<Renderer>().GetDevice())
		{
			Cache.EvictUnused(Device);
		}

		AssetCache<GPUMesh>& Cache;
		GfxDevice&           Device;
	};
}

TEST_CASE("A released asset stays cached and is reused by the next acquire")
{
	MeshCache m;
	const AssetKey key("engine://Content/Meshes/Sphere.kmesh");
	m.Cache.Collect(m.Device, 100.0, kGrace);

	const MeshHandle first = m.Cache.Acquire(m.Device, key);
	m.Cache.Release(m.Device, first);
	CHECK(m.Cache.IsResident(key));
	CHECK(m.Cache.GetRefCount(key) == 0);

	const MeshHandle second = m.Cache.Acquire(m.Device, key);
	CHECK(second == first);  // same slot and generation: nothing was reloaded
	CHECK(m.Cache.GetRefCount(key) == 1);

	m.Cache.Release(m.Device, second);
}

TEST_CASE("Collect evicts an unused asset only once the grace period has passed")
{
	MeshCache m;
	const AssetKey key("engine://Content/Meshes/Cylinder.kmesh");
	m.Cache.Collect(m.Device, 200.0, kGrace);

	m.Cache.Release(m.Device, m.Cache.Acquire(m.Device, key));

	m.Cache.Collect(m.Device, 204.0, kGrace);
	CHECK(m.Cache.IsResident(key));

	m.Cache.Collect(m.Device, 205.0, kGrace);
	CHECK_FALSE(m.Cache.IsResident(key));
}

TEST_CASE("An asset acquired again within the grace period survives later collects")
{
	MeshCache m;
	const AssetKey key("engine://Content/Meshes/Torus.kmesh");
	m.Cache.Collect(m.Device, 300.0, kGrace);

	m.Cache.Release(m.Device, m.Cache.Acquire(m.Device, key));
	const MeshHandle held = m.Cache.Acquire(m.Device, key);

	m.Cache.Collect(m.Device, 400.0, kGrace);
	CHECK(m.Cache.IsResident(key));
	CHECK(m.Cache.Get(held) != nullptr);

	// Released again: the clock starts over from the latest release
	m.Cache.Release(m.Device, held);
	m.Cache.Collect(m.Device, 403.0, kGrace);
	CHECK(m.Cache.IsResident(key));
	m.Cache.Collect(m.Device, 405.0, kGrace);
	CHECK_FALSE(m.Cache.IsResident(key));
}

TEST_CASE("EvictIfUnused drops a stale cached copy but never a referenced one")
{
	MeshCache m;
	const AssetKey key("engine://Content/Meshes/Cone.kmesh");

	const MeshHandle held = m.Cache.Acquire(m.Device, key);
	m.Cache.EvictIfUnused(m.Device, key);
	CHECK(m.Cache.IsResident(key));

	m.Cache.Release(m.Device, held);
	m.Cache.EvictIfUnused(m.Device, key);
	CHECK_FALSE(m.Cache.IsResident(key));
}

TEST_CASE("The pinned fallback mesh is never evicted")
{
	MeshCache m;
	const AssetKey box("engine://Content/Meshes/Box.kmesh");
	REQUIRE(m.Cache.IsResident(box));

	m.Cache.EvictUnused(m.Device);
	CHECK(m.Cache.IsResident(box));
	CHECK(m.Cache.GetRefCount(box) >= 1);
}
