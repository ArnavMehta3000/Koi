#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <doctest/doctest.h>

using namespace koi;

namespace
{
	template <typename T>
	T RoundTrip(const T& value)
	{
		toml::table table;
		utils::SerializeInto(table, "value", value);
		const std::optional<T> loaded = utils::DeserializeFrom<T>(table, "value");
		REQUIRE(loaded);
		return *loaded;
	}
}

TEST_CASE("f32 survives TOML's f64 storage")
{
	CHECK(RoundTrip(0.1f) == 0.1f);
}

TEST_CASE("Transform round-trips through reflect-cpp")
{
	const Transform in{ .Position = Vector3(1, 2, 3), .Rotation = Quaternion(0, 0.7071f, 0, 0.7071f), .Scale = Vector3(2, 2, 2) };
	const Transform out = RoundTrip(in);

	CHECK(out.Position == in.Position);
	CHECK(out.Rotation == in.Rotation);
	CHECK(out.Scale == in.Scale);
}

TEST_CASE("Light keys are snake_case and enums round-trip")
{
	const Light in{ .Type = LightType::Directional, .Intensity = 3.0f, .CastsShadows = true };
	const toml::table saved = Serializer<Light>::Serialize(in);

	CHECK(saved.contains("casts_shadows"));
	CHECK(saved.contains("shadow_bias"));

	const Light out = RoundTrip(in);
	CHECK(out.Type == LightType::Directional);
	CHECK(out.Intensity == 3.0f);
	CHECK(out.CastsShadows);
}

TEST_CASE("Missing keys fall back to field defaults")
{
	const toml::table partial{ { "intensity", 5.0 } };
	const std::optional<Light> light = Serializer<Light>::Deserialize(partial);

	REQUIRE(light);
	CHECK(light->Intensity == 5.0f);
	CHECK(light->Range == Light{}.Range);
	CHECK(light->Type == Light{}.Type);
}
