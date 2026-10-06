#pragma once
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Core/EnumFlags.h"
#include <optional>
#include <string_view>

namespace koi
{
	// Key values match GLFW_KEY_* internally
	enum class KeyCode : i32
	{
		Unknown = -1,

		Space       = 32,
		Apostrophe  = 39,
		Comma       = 44,
		Minus       = 45,
		Period      = 46,
		Slash       = 47,

		Num0 = 48, Num1, Num2, Num3, Num4,
		Num5, Num6, Num7, Num8, Num9,

		Semicolon = 59,
		Equal     = 61,

		A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
		N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

		LeftBracket  = 91,
		Backslash    = 92,
		RightBracket = 93,
		GraveAccent  = 96,

		World1 = 161,
		World2 = 162,

		Escape    = 256,
		Enter     = 257,
		Tab       = 258,
		Backspace = 259,
		Insert    = 260,
		Delete    = 261,
		Right     = 262,
		Left      = 263,
		Down      = 264,
		Up        = 265,
		PageUp    = 266,
		PageDown  = 267,
		Home      = 268,
		End       = 269,

		CapsLock    = 280,
		ScrollLock  = 281,
		NumLock     = 282,
		PrintScreen = 283,
		Pause       = 284,

		F1 = 290, F2,  F3,  F4,  F5,  F6,  F7,
		F8,  F9,  F10, F11, F12, F13, F14,
		F15, F16, F17, F18, F19, F20, F21,
		F22, F23, F24, F25,

		KP0 = 320, KP1, KP2, KP3, KP4,
		KP5, KP6, KP7, KP8, KP9,
		KPDecimal  = 330,
		KPDivide   = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd      = 334,
		KPEnter    = 335,
		KPEqual    = 336,

		LeftShift    = 340,
		LeftControl  = 341,
		LeftAlt      = 342,
		LeftSuper    = 343,
		RightShift   = 344,
		RightControl = 345,
		RightAlt     = 346,
		RightSuper   = 347,
		Menu         = 348,
	};

	enum class MouseButton : i32
	{
		Left   = 0,
		Right  = 1,
		Middle = 2,
		B4     = 3,
		B5     = 4,
		B6     = 5,
		B7     = 6,
		B8     = 7,
		Count  = 8,
	};

	// Modifier values match GLFW_MOD_* internally - do not rely on the numeric values in game code.
	enum class Modifier : u8
	{
		None     = 0x00,
		Shift    = 0x01,
		Control  = 0x02,
		Alt      = 0x04,
		Super    = 0x08,
		CapsLock = 0x10,
		NumLock  = 0x20,
	};
}
KOI_ENABLE_ENUM_FLAGS(koi::Modifier);

namespace koi
{
	// Values match GLFW_GAMEPAD_BUTTON_* / GLFW_GAMEPAD_AXIS_* internally
	enum class GamepadButton : u8
	{
		A, B, X, Y,
		LeftBumper, RightBumper,
		Back, Start, Guide,
		LeftThumb, RightThumb,
		DPadUp, DPadRight, DPadDown, DPadLeft,
		Count
	};

	enum class GamepadAxis : u8
	{
		LeftX, LeftY, RightX, RightY,
		LeftTrigger, RightTrigger,
		Count
	};

	namespace utils
	{
		[[nodiscard]] std::optional<KeyCode>       ParseKeyCode(std::string_view name);
		[[nodiscard]] std::optional<GamepadButton> ParseGamepadButton(std::string_view name);
		[[nodiscard]] std::optional<GamepadAxis>   ParseGamepadAxis(std::string_view name);
	}
}
