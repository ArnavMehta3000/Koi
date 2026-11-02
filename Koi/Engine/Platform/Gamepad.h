#pragma once
#include "Koi/Engine/Platform/InputActions.h"
#include "Koi/Engine/Platform/InputCodes.h"
#include <array>

namespace koi
{
	// The first connected gamepad only | Axes are deadzoned and in [-1, 1] (triggers [0, 1])
	class Gamepad : public NonCopyable, public NonMovable
	{
	public:
		[[nodiscard]] inline bool IsConnected() const noexcept { return m_connected; }
		[[nodiscard]] bool IsButtonDown(GamepadButton btn)     const noexcept;
		[[nodiscard]] bool IsButtonPressed(GamepadButton btn)  const noexcept;
		[[nodiscard]] bool IsButtonReleased(GamepadButton btn) const noexcept;
		[[nodiscard]] f32  GetAxis(GamepadAxis axis) const noexcept;

	private:
		friend class InputManager;

		void Init(const GamepadSettings& settings) { m_settings = settings; }
		void Poll();

	private:
		static constexpr u32 kMaxButton = static_cast<u32>(GamepadButton::Count);
		static constexpr u32 kMaxAxis   = static_cast<u32>(GamepadAxis::Count);

		GamepadSettings              m_settings;
		std::array<bool, kMaxButton> m_down{};
		std::array<bool, kMaxButton> m_downPrev{};
		std::array<f32,  kMaxAxis>   m_axes{};
		bool                         m_connected = false;
	};
}
