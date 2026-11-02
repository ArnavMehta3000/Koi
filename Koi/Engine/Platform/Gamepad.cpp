#include "Koi/Engine/Platform/Gamepad.h"

namespace koi
{
	bool Gamepad::IsButtonDown(GamepadButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxButton && m_down[i];
	}

	bool Gamepad::IsButtonPressed(GamepadButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxButton && m_down[i] && !m_downPrev[i];
	}

	bool Gamepad::IsButtonReleased(GamepadButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxButton && !m_down[i] && m_downPrev[i];
	}

	f32 Gamepad::GetAxis(GamepadAxis axis) const noexcept
	{
		const u32 i = static_cast<u32>(axis);
		return i < kMaxAxis ? m_axes[i] : 0.0f;
	}

	void Gamepad::Poll()
	{
		m_downPrev = m_down;

		// [TODO]: GameInput
		m_connected = false;
		m_down.fill(false);
		m_axes.fill(0.0f);
	}
}
