#include "Koi/Engine/Platform/Keyboard.h"
#include "Koi/Engine/Platform/Win32.h"

namespace koi
{
	i32 Keyboard::IndexOf(KeyCode key) noexcept
	{
		const i32 index = static_cast<i32>(key);
		return index >= 0 && index < static_cast<i32>(kMaxKeyCode) ? index : -1;
	}

	bool Keyboard::IsDown(KeyCode key) const noexcept
	{
		const i32 i = IndexOf(key);
		return i >= 0 && m_down[i];
	}

	bool Keyboard::IsPressed(KeyCode key) const noexcept
	{
		const i32 i = IndexOf(key);
		return i >= 0 && m_down[i] && !m_downPrev[i];
	}

	bool Keyboard::IsReleased(KeyCode key) const noexcept
	{
		const i32 i = IndexOf(key);
		return i >= 0 && !m_down[i] && m_downPrev[i];
	}

	bool Keyboard::IsModifierDown(Modifier mod) const noexcept
	{
		switch (mod)
		{
		case Modifier::Shift:    return IsDown(KeyCode::LeftShift)   || IsDown(KeyCode::RightShift);
		case Modifier::Control:  return IsDown(KeyCode::LeftControl) || IsDown(KeyCode::RightControl);
		case Modifier::Alt:      return IsDown(KeyCode::LeftAlt)     || IsDown(KeyCode::RightAlt);
		case Modifier::Super:    return IsDown(KeyCode::LeftSuper)   || IsDown(KeyCode::RightSuper);
		case Modifier::CapsLock: return ::GetKeyState(VK_CAPITAL) & 1;
		case Modifier::NumLock:  return ::GetKeyState(VK_NUMLOCK) & 1;
		default:                 return false;
		}
	}

	void Keyboard::OnRawInput(const RAWKEYBOARD& raw) noexcept
	{
		if (raw.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || raw.VKey == 0xFF) return;  // 0xFF: fake key inside an escape sequence

		KeyCode key = KeyCode::Pause;  // E1 1D 45, the only E1 sequence
		if (raw.VKey != VK_PAUSE)
		{
			const u16 scancode = raw.MakeCode != 0
				? static_cast<u16>(raw.MakeCode | ((raw.Flags & RI_KEY_E0) ? 0xE000 : 0))
				: static_cast<u16>(::MapVirtualKeyW(raw.VKey, MAPVK_VK_TO_VSC_EX));
			key = utils::KeyCodeFromScancode(scancode);
		}

		if (const i32 i = IndexOf(key); i >= 0)
		{
			m_down[i] = (raw.Flags & RI_KEY_BREAK) == 0;
		}
	}

	void Keyboard::ReleaseAll() noexcept
	{
		m_down.fill(false);
	}
}
