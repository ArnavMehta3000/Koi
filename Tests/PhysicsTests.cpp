#include <box3d/box3d.h>
#include <doctest/doctest.h>

// Only proves the box3d package builds and links in this mode; the engine has no physics yet
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
