#include "Koi/Engine/Platform/Gamepad.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/MathTypes.h"
#include "Koi/Engine/Platform/Win32.h"
#include <GameInput.h>

#include <algorithm>

namespace koi
{
	namespace
	{
		namespace gi = GameInput::v3;

		constexpr std::array<std::pair<GamepadButton, gi::GameInputGamepadButtons>, 14> kButtons
		{{
			{ GamepadButton::A,           gi::GameInputGamepadA              },
			{ GamepadButton::B,           gi::GameInputGamepadB              },
			{ GamepadButton::X,           gi::GameInputGamepadX              },
			{ GamepadButton::Y,           gi::GameInputGamepadY              },
			{ GamepadButton::LeftBumper,  gi::GameInputGamepadLeftShoulder   },
			{ GamepadButton::RightBumper, gi::GameInputGamepadRightShoulder  },
			{ GamepadButton::Back,        gi::GameInputGamepadView           },
			{ GamepadButton::Start,       gi::GameInputGamepadMenu           },
			{ GamepadButton::LeftThumb,   gi::GameInputGamepadLeftThumbstick },
			{ GamepadButton::RightThumb,  gi::GameInputGamepadRightThumbstick},
			{ GamepadButton::DPadUp,      gi::GameInputGamepadDPadUp         },
			{ GamepadButton::DPadRight,   gi::GameInputGamepadDPadRight      },
			{ GamepadButton::DPadDown,    gi::GameInputGamepadDPadDown       },
			{ GamepadButton::DPadLeft,    gi::GameInputGamepadDPadLeft       },
		}};
	}

	Gamepad::~Gamepad()
	{
		Disconnect();
		if (m_gameInput)
		{
			m_gameInput->Release();
			m_gameInput = nullptr;
		}
	}

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

	void Gamepad::SetRumble(const GamepadRumble& rumble) noexcept
	{
		m_rumble =
		{
			.Low          = std::clamp(rumble.Low,          0.0f, 1.0f),
			.High         = std::clamp(rumble.High,         0.0f, 1.0f),
			.LeftTrigger  = std::clamp(rumble.LeftTrigger,  0.0f, 1.0f),
			.RightTrigger = std::clamp(rumble.RightTrigger, 0.0f, 1.0f),
		};
	}

	void Gamepad::Init(const GamepadSettings& settings)
	{
		m_settings = settings;

		const HRESULT hr = gi::GameInputCreate(&m_gameInput);
		if (SUCCEEDED(hr))
		{
			KOI_LOG(Input, Debug, "GameInput initialized");
			return;
		}

		m_gameInput = nullptr;
		const char* reason = hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ? "the GameInput runtime is not installed"
			: hr == E_NOINTERFACE ? "the installed GameInput runtime is too old"
			: "GameInput failed to start";
		KOI_LOG(Input, Warn, "Gamepads unavailable: {} (0x{:08X}). Install GameInputRedist.msi from https://github.com/microsoftconnect/GameInput/releases",
			reason, static_cast<u32>(hr));
	}

	void Gamepad::Poll(bool allowRumble)
	{
		m_downPrev = m_down;
		m_down.fill(false);
		m_axes.fill(0.0f);

		if (!m_gameInput) return;

		if (m_device && !(m_device->GetDeviceStatus() & gi::GameInputDeviceConnected))
		{
			KOI_LOG(Input, Info, "Gamepad disconnected");
			Disconnect();
		}
		ApplyRumble(allowRumble ? m_rumble : GamepadRumble{});

		// Latches onto whichever pad reports first, then sticks with it
		gi::IGameInputReading* reading = nullptr;
		if (FAILED(m_gameInput->GetCurrentReading(gi::GameInputKindGamepad, m_device, &reading)))
		{
			return;
		}

		if (!m_device)
		{
			reading->GetDevice(&m_device);

			const gi::GameInputDeviceInfo* info = nullptr;
			const bool named = SUCCEEDED(m_device->GetDeviceInfo(&info)) && info->displayName;
			KOI_LOG(Input, Info, "Gamepad connected: {}", named ? info->displayName : "<unnamed>");
		}

		gi::GameInputGamepadState state{};
		const bool valid = reading->GetGamepadState(&state);
		reading->Release();
		if (!valid) return;

		for (const auto& [button, flag] : kButtons)
		{
			m_down[static_cast<u32>(button)] = (state.buttons & flag) != 0;
		}

		// Sticks: radial deadzone, rescaled so the edge of the zone reads as zero
		const auto stick = [&](GamepadAxis x, GamepadAxis y, f32 rawX, f32 rawY)
		{
			Vector2 v(rawX, rawY);
			const f32 length   = v.Length();
			const f32 deadzone = m_settings.StickDeadzone;

			if (length <= deadzone) v = Vector2::Zero;
			else v = v * ((length - deadzone) / ((1.0f - deadzone) * length));

			m_axes[static_cast<u32>(x)] = std::clamp(v.x, -1.0f, 1.0f);
			m_axes[static_cast<u32>(y)] = std::clamp(v.y, -1.0f, 1.0f);
		};
		stick(GamepadAxis::LeftX,  GamepadAxis::LeftY,  state.leftThumbstickX,  state.leftThumbstickY);
		stick(GamepadAxis::RightX, GamepadAxis::RightY, state.rightThumbstickX, state.rightThumbstickY);

		// Triggers: [0, 1], rescaled past the deadzone
		const auto trigger = [&](GamepadAxis axis, f32 raw)
		{
			const f32 deadzone = m_settings.TriggerDeadzone;
			m_axes[static_cast<u32>(axis)] = raw <= deadzone ? 0.0f : (raw - deadzone) / (1.0f - deadzone);
		};
		trigger(GamepadAxis::LeftTrigger,  state.leftTrigger);
		trigger(GamepadAxis::RightTrigger, state.rightTrigger);
	}

	void Gamepad::ApplyRumble(const GamepadRumble& rumble)
	{
		if (!m_device || rumble == m_applied) return;

		const gi::GameInputRumbleParams params
		{
			.lowFrequency  = rumble.Low,
			.highFrequency = rumble.High,
			.leftTrigger   = rumble.LeftTrigger,
			.rightTrigger  = rumble.RightTrigger,
		};
		m_device->SetRumbleState(&params);
		m_applied = rumble;
	}

	void Gamepad::Disconnect()
	{
		if (!m_device) return;

		m_device->SetRumbleState(nullptr);
		m_device->Release();
		m_device  = nullptr;
		m_applied = {};
	}
}
