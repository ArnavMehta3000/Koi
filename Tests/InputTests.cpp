#include "Koi/Engine/Platform/InputActions.h"
#include <doctest/doctest.h>
#include <toml++/toml.hpp>

using namespace koi;

TEST_CASE("InputConfig reads deadzones, bindings and inverted axes")
{
	const toml::table input = toml::parse(R"(
		stick_deadzone = 0.3

		[actions.MoveZ]
		negative = [ "S" ]
		positive = [ "W", "NotAKey" ]
		axes     = [ "-Gamepad.LeftY" ]
	)");

	const InputConfig config = InputConfig::Load(input);
	CHECK(config.Gamepad.StickDeadzone == doctest::Approx(0.3f));
	CHECK(config.Gamepad.TriggerDeadzone == GamepadSettings{}.TriggerDeadzone);

	REQUIRE(config.Actions.size() == 1);
	const InputAction& move = config.Actions[0];
	CHECK(move.Name == "MoveZ");
	REQUIRE(move.Bindings.size() == 3);  // the unknown key is dropped

	// positive first, then negative, then axes
	CHECK(std::get<KeyCode>(move.Bindings[0].Source) == KeyCode::W);
	CHECK(move.Bindings[0].Scale == 1.0f);
	CHECK(std::get<KeyCode>(move.Bindings[1].Source) == KeyCode::S);
	CHECK(move.Bindings[1].Scale == -1.0f);
	CHECK(std::get<GamepadAxis>(move.Bindings[2].Source) == GamepadAxis::LeftY);
	CHECK(move.Bindings[2].Scale == -1.0f);
}
