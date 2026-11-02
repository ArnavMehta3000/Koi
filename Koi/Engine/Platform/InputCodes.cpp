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

		// Indexed by make code, +0x100 for E0-prefixed keys
		constexpr std::array<KeyCode, 0x200> kScancodes = []
		{
			std::array<KeyCode, 0x200> t{};
			t.fill(KeyCode::Unknown);

			using enum KeyCode;
			constexpr KeyCode row1[] = { Escape, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0, Minus, Equal, Backspace, Tab };
			constexpr KeyCode row2[] = { Q, W, E, R, T, Y, U, I, O, P, LeftBracket, RightBracket, Enter, LeftControl };
			constexpr KeyCode row3[] = { A, S, D, F, G, H, J, K, L, Semicolon, Apostrophe, GraveAccent, LeftShift, Backslash };
			constexpr KeyCode row4[] = { Z, X, C, V, B, N, M, Comma, Period, Slash, RightShift, KPMultiply, LeftAlt, Space, CapsLock };
			constexpr KeyCode pad[]  = { NumLock, ScrollLock, KP7, KP8, KP9, KPSubtract, KP4, KP5, KP6, KPAdd, KP1, KP2, KP3, KP0, KPDecimal };

			const auto run = [&t](u16 first, const auto& keys) { for (KeyCode key : keys) t[first++] = key; };
			run(0x01, row1);
			run(0x10, row2);
			run(0x1E, row3);
			run(0x2C, row4);
			run(0x45, pad);
			for (u16 i = 0; i < 10; ++i) t[0x3B + i] = static_cast<KeyCode>(static_cast<i32>(F1)  + i);
			for (u16 i = 0; i < 11; ++i) t[0x64 + i] = static_cast<KeyCode>(static_cast<i32>(F13) + i);

			t[0x54] = PrintScreen;  // Alt+PrintScreen
			t[0x56] = World2;       // ISO key between left shift and Z
			t[0x57] = F11;
			t[0x58] = F12;
			t[0x59] = KPEqual;
			t[0x76] = F24;

			t[0x11C] = KPEnter;     t[0x11D] = RightControl; t[0x135] = KPDivide;  t[0x137] = PrintScreen;
			t[0x138] = RightAlt;    t[0x145] = NumLock;      t[0x146] = Pause;     t[0x147] = Home;
			t[0x148] = Up;          t[0x149] = PageUp;       t[0x14B] = Left;      t[0x14D] = Right;
			t[0x14F] = End;         t[0x150] = Down;         t[0x151] = PageDown;  t[0x152] = Insert;
			t[0x153] = Delete;      t[0x15B] = LeftSuper;    t[0x15C] = RightSuper; t[0x15D] = Menu;
			return t;
		}();
	}

	KeyCode utils::KeyCodeFromScancode(u16 scancode) noexcept
	{
		const u16 high = scancode >> 8;
		if (high != 0 && high != 0xE0) return KeyCode::Unknown;
		return kScancodes[(scancode & 0xFF) | (high ? 0x100 : 0)];
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
