#include "Koi/Engine/Platform/InputCodes.h"
#include <array>
#include <utility>

namespace koi
{
	namespace
	{
		constexpr std::array kNamedKeys
		{
			std::pair{ "Space"       , KeyCode::Space        }, std::pair{ "Apostrophe", KeyCode::Apostrophe     },
			std::pair{ "Comma"       , KeyCode::Comma        }, std::pair{ "Minus", KeyCode::Minus               },
			std::pair{ "Period"      , KeyCode::Period       }, std::pair{ "Slash", KeyCode::Slash               },
			std::pair{ "Semicolon"   , KeyCode::Semicolon    }, std::pair{ "Equal", KeyCode::Equal               },
			std::pair{ "LeftBracket" , KeyCode::LeftBracket  }, std::pair{ "Backslash", KeyCode::Backslash       },
			std::pair{ "RightBracket", KeyCode::RightBracket }, std::pair{ "GraveAccent", KeyCode::GraveAccent   },
			std::pair{ "Escape"      , KeyCode::Escape       }, std::pair{ "Enter", KeyCode::Enter               },
			std::pair{ "Tab"         , KeyCode::Tab          }, std::pair{ "Backspace", KeyCode::Backspace       },
			std::pair{ "Insert"      , KeyCode::Insert       }, std::pair{ "Delete", KeyCode::Delete             },
			std::pair{ "Right"       , KeyCode::Right        }, std::pair{ "Left", KeyCode::Left                 },
			std::pair{ "Down"        , KeyCode::Down         }, std::pair{ "Up", KeyCode::Up                     },
			std::pair{ "PageUp"      , KeyCode::PageUp       }, std::pair{ "PageDown", KeyCode::PageDown         },
			std::pair{ "Home"        , KeyCode::Home         }, std::pair{ "End", KeyCode::End                   },
			std::pair{ "CapsLock"    , KeyCode::CapsLock     }, std::pair{ "ScrollLock", KeyCode::ScrollLock     },
			std::pair{ "NumLock"     , KeyCode::NumLock      }, std::pair{ "PrintScreen", KeyCode::PrintScreen   },
			std::pair{ "Pause"       , KeyCode::Pause        },
			std::pair{ "KPDecimal"   , KeyCode::KPDecimal    }, std::pair{ "KPDivide", KeyCode::KPDivide         },
			std::pair{ "KPMultiply"  , KeyCode::KPMultiply   }, std::pair{ "KPSubtract", KeyCode::KPSubtract     },
			std::pair{ "KPAdd"       , KeyCode::KPAdd        }, std::pair{ "KPEnter", KeyCode::KPEnter           },
			std::pair{ "KPEqual"     , KeyCode::KPEqual      },
			std::pair{ "LeftShift"   , KeyCode::LeftShift    }, std::pair{ "LeftControl", KeyCode::LeftControl   },
			std::pair{ "LeftAlt"     , KeyCode::LeftAlt      }, std::pair{ "LeftSuper", KeyCode::LeftSuper       },
			std::pair{ "RightShift"  , KeyCode::RightShift   }, std::pair{ "RightControl", KeyCode::RightControl },
			std::pair{ "RightAlt"    , KeyCode::RightAlt     }, std::pair{ "RightSuper", KeyCode::RightSuper     },
			std::pair{ "Menu"        , KeyCode::Menu         },
		};

		constexpr std::array kGamepadButtons
		{
			std::pair{ "A", GamepadButton::A                   }, std::pair{ "B", GamepadButton::B                     },
			std::pair{ "X", GamepadButton::X                   }, std::pair{ "Y", GamepadButton::Y                     },
			std::pair{ "LeftBumper", GamepadButton::LeftBumper }, std::pair{ "RightBumper", GamepadButton::RightBumper },
			std::pair{ "Back", GamepadButton::Back             }, std::pair{ "Start", GamepadButton::Start             },
			std::pair{ "Guide", GamepadButton::Guide           },
			std::pair{ "LeftThumb", GamepadButton::LeftThumb   }, std::pair{ "RightThumb", GamepadButton::RightThumb   },
			std::pair{ "DPadUp", GamepadButton::DPadUp         }, std::pair{ "DPadRight", GamepadButton::DPadRight     },
			std::pair{ "DPadDown", GamepadButton::DPadDown     }, std::pair{ "DPadLeft", GamepadButton::DPadLeft       },
		};

		constexpr std::array kGamepadAxes
		{
			std::pair{ "LeftX", GamepadAxis::LeftX             }, std::pair{ "LeftY", GamepadAxis::LeftY               },
			std::pair{ "RightX", GamepadAxis::RightX           }, std::pair{ "RightY", GamepadAxis::RightY             },
			std::pair{ "LeftTrigger", GamepadAxis::LeftTrigger }, std::pair{ "RightTrigger", GamepadAxis::RightTrigger },
		};

		template<typename T, size_t N>
		std::optional<T> Lookup(const std::array<std::pair<const char*, T>, N>& table, std::string_view name)
		{
			for (const auto& [key, value] : table)
			{
				if (name == key) return value;
			}
			return std::nullopt;
		}

		// Number parsing for "Num0".."Num9", "KP0".."KP9", "F1".."F25"
		std::optional<i32> TrailingNumber(std::string_view name, std::string_view prefix, i32 max)
		{
			if (!name.starts_with(prefix) || name.size() == prefix.size()) return std::nullopt;

			i32 value = 0;
			for (const char c : name.substr(prefix.size()))
			{
				if (c < '0' || c > '9') return std::nullopt;
				value = value * 10 + (c - '0');
			}
			return value <= max ? std::optional(value) : std::nullopt;
		}
	}

	std::optional<KeyCode> utils::ParseKeyCode(std::string_view name)
	{
		if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z')
		{
			return static_cast<KeyCode>(static_cast<i32>(KeyCode::A) + (name[0] - 'A'));
		}

		if (const auto n = TrailingNumber(name, "Num", 9)) return static_cast<KeyCode>(static_cast<i32>(KeyCode::Num0) + *n);
		if (const auto n = TrailingNumber(name, "KP", 9))  return static_cast<KeyCode>(static_cast<i32>(KeyCode::KP0) + *n);
		if (const auto n = TrailingNumber(name, "F", 25); n && *n >= 1) return static_cast<KeyCode>(static_cast<i32>(KeyCode::F1) + *n - 1);

		return Lookup(kNamedKeys, name);
	}

	std::optional<GamepadButton> utils::ParseGamepadButton(std::string_view name) { return Lookup(kGamepadButtons, name); }
	std::optional<GamepadAxis>   utils::ParseGamepadAxis(std::string_view name)   { return Lookup(kGamepadAxes, name); }
}
