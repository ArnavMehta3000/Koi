#include "Koi/Engine/Platform/InputActions.h"
#include "Koi/Engine/Logging/Logger.h"
#include <toml++/toml.hpp>

namespace koi
{
	namespace
	{
		constexpr std::string_view kGamepadPrefix = "Gamepad.";

		// "Space" | "Gamepad.A" | "Gamepad.LeftX" | "-Gamepad.LeftY"
		std::optional<InputBinding> ParseBinding(std::string_view text, f32 scale)
		{
			if (text.starts_with('-'))
			{
				scale = -scale;
				text.remove_prefix(1);
			}

			if (text.starts_with(kGamepadPrefix))
			{
				const std::string_view name = text.substr(kGamepadPrefix.size());
				if (const auto button = utils::ParseGamepadButton(name)) return InputBinding{ *button, scale };
				if (const auto axis   = utils::ParseGamepadAxis(name))   return InputBinding{ *axis, scale };
				return std::nullopt;
			}

			if (const auto key = utils::ParseKeyCode(text)) return InputBinding{ *key, scale };
			return std::nullopt;
		}

		void AddBindings(InputAction& action, const toml::array* list, f32 scale)
		{
			if (!list) return;

			for (const toml::node& node : *list)
			{
				const std::optional<std::string> text = node.value<std::string>();
				if (!text) continue;

				if (const auto binding = ParseBinding(*text, scale))
				{
					action.Bindings.push_back(*binding);
				}
				else
				{
					KOI_LOG_WARN("Input: action '{}' has unknown binding '{}'", action.Name, *text);
				}
			}
		}
	}

	InputConfig InputConfig::Load(const toml::table& input)
	{
		InputConfig config{};
		config.Gamepad.StickDeadzone   = input["stick_deadzone"].value_or(config.Gamepad.StickDeadzone);
		config.Gamepad.TriggerDeadzone = input["trigger_deadzone"].value_or(config.Gamepad.TriggerDeadzone);

		const toml::table* actions = input["actions"].as_table();
		if (!actions) return config;

		for (const auto& [name, node] : *actions)
		{
			const toml::table* entry = node.as_table();
			if (!entry) continue;

			InputAction action{ .Name = std::string(name.str()) };
			AddBindings(action, (*entry)["positive"].as_array(),  1.0f);
			AddBindings(action, (*entry)["buttons"].as_array(),   1.0f);  // alias of positive
			AddBindings(action, (*entry)["negative"].as_array(), -1.0f);
			AddBindings(action, (*entry)["axes"].as_array(),      1.0f);

			config.Actions.push_back(std::move(action));
		}

		KOI_LOG_DEBUG("Input: {} action(s) loaded", config.Actions.size());
		return config;
	}
}
