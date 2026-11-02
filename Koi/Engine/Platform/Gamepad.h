#pragma once
#include "Koi/Engine/Platform/InputActions.h"
#include "Koi/Engine/Platform/InputCodes.h"
#include <array>

namespace GameInput::v3
{
	struct IGameInput;
	struct IGameInputDevice;
}

namespace koi
{
	// Motor strengths in [0, 1] | Trigger motors exist on Xbox One style pads only
	struct GamepadRumble
	{
		f32 Low          = 0.0f;
		f32 High         = 0.0f;
		f32 LeftTrigger  = 0.0f;
		f32 RightTrigger = 0.0f;

		bool operator==(const GamepadRumble&) const = default;
	};

	// The first connected gamepad only | Axes are deadzoned and in [-1, 1] with up/right positive (triggers [0, 1])
	class Gamepad : public NonCopyable, public NonMovable
	{
	public:
		~Gamepad();

		[[nodiscard]] inline bool IsConnected() const noexcept { return m_device != nullptr; }
		[[nodiscard]] bool IsButtonDown(GamepadButton btn)     const noexcept;
		[[nodiscard]] bool IsButtonPressed(GamepadButton btn)  const noexcept;
		[[nodiscard]] bool IsButtonReleased(GamepadButton btn) const noexcept;
		[[nodiscard]] f32  GetAxis(GamepadAxis axis) const noexcept;

		void SetRumble(const GamepadRumble& rumble) noexcept;
		[[nodiscard]] inline const GamepadRumble& GetRumble() const noexcept { return m_rumble; }

	private:
		friend class InputManager;

		void Init(const GamepadSettings& settings);
		void Poll(bool allowRumble);
		void Disconnect();
		void ApplyRumble(const GamepadRumble& rumble);

	private:
		static constexpr u32 kMaxButton = static_cast<u32>(GamepadButton::Count);
		static constexpr u32 kMaxAxis   = static_cast<u32>(GamepadAxis::Count);

		GameInput::v3::IGameInput*       m_gameInput = nullptr;
		GameInput::v3::IGameInputDevice* m_device    = nullptr;

		GamepadSettings              m_settings;
		std::array<bool, kMaxButton> m_down{};
		std::array<bool, kMaxButton> m_downPrev{};
		std::array<f32,  kMaxAxis>   m_axes{};

		GamepadRumble m_rumble;   // what the game asked for
		GamepadRumble m_applied;  // what the motors are doing
	};
}
