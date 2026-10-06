#pragma once
#include "Koi/Engine/Platform/InputCodes.h"
#include <string>
#include <variant>
#include <vector>

namespace toml { inline namespace v3 { class table; } }

namespace koi
{
	struct GamepadSettings
	{
		f32 StickDeadzone   = 0.2f;   // radial, per stick
		f32 TriggerDeadzone = 0.05f;
	};

	// One source feeding an action
	struct InputBinding
	{
		std::variant<KeyCode, GamepadButton, GamepadAxis> Source;
		f32 Scale = 1.0f;
	};

	// Every action is an axis in [-1, 1]; a button action is just one with only positive bindings
	struct InputAction
	{
		std::string               Name;
		std::vector<InputBinding> Bindings;
	};

	struct InputConfig
	{
		GamepadSettings          Gamepad;
		std::vector<InputAction> Actions;

		[[nodiscard]] static InputConfig Load(const toml::table& input);
	};
}
