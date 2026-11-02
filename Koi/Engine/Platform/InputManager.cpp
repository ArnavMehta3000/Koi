#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

#include <algorithm>
#include <cmath>

namespace koi
{
	InputManager::InputManager()
	{
		KOI_ASSERT(s_instance == nullptr, "InputManager already exists");
		s_instance = this;
	}

	InputManager::~InputManager()
	{
		if (m_window)
		{
			m_window->RemoveMessageHook(m_hookId);
			m_mouse.Shutdown();

			const RAWINPUTDEVICE devices[]
			{
				{ .usUsagePage = 0x01, .usUsage = 0x02, .dwFlags = RIDEV_REMOVE, .hwndTarget = nullptr },
				{ .usUsagePage = 0x01, .usUsage = 0x06, .dwFlags = RIDEV_REMOVE, .hwndTarget = nullptr },
			};
			::RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
		}
		s_instance = nullptr;
	}

	void InputManager::Init(const SubsystemInitContext& ctx)
	{
		ZoneScoped;

		m_window = &ctx.MainWindow;
		const HWND hwnd = static_cast<HWND>(m_window->GetNativeHandle());
		KOI_ASSERT(hwnd, "Window has no native HWND");

		m_config = ctx.Input;
		m_actions.assign(m_config.Actions.size(), ActionState{});

		// Mouse (usage 2) and keyboard (usage 6) on the generic desktop page, delivered while the app is in the foreground
		const RAWINPUTDEVICE devices[]
		{
			{ .usUsagePage = 0x01, .usUsage = 0x02, .dwFlags = 0, .hwndTarget = hwnd },
			{ .usUsagePage = 0x01, .usUsage = 0x06, .dwFlags = 0, .hwndTarget = hwnd },
		};
		if (!::RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)))
		{
			KOI_LOG(Input, Error, "Failed to register raw input devices (error {})", ::GetLastError());
		}

		m_mouse.Init(hwnd);
		m_gamepad.Init(m_config.Gamepad);
		m_hookId = m_window->AddMessageHook([this](const WindowMessage& msg) { return OnMessage(msg); });

		KOI_LOG(Input, Debug, "InputManager initialized");
	}

	void InputManager::BeginFrame()
	{
		ZoneScoped;

		m_keyboard.BeginFrame();
		m_mouse.BeginFrame();
		m_gamepad.Poll();
		EvaluateActions();
	}

	std::optional<i64> InputManager::OnMessage(const WindowMessage& msg)
	{
		switch (msg.Msg)
		{
		case WM_INPUT:
			OnRawInput(msg.LParam);
			return std::nullopt;  // DefWindowProc still has to clean up after WM_INPUT

		case WM_ACTIVATEAPP:
			// Key-ups that land while another app is in front never reach us
			if (msg.WParam == FALSE) m_keyboard.ReleaseAll();
			m_mouse.OnAppActivated(msg.WParam != FALSE);
			return std::nullopt;

		default:
			return m_mouse.OnMessage(msg);
		}
	}

	void InputManager::OnRawInput(i64 handle)
	{
		RAWINPUT raw{};
		UINT size = sizeof(raw);
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(handle), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
		{
			return;
		}

		if (raw.header.dwType == RIM_TYPEKEYBOARD)   m_keyboard.OnRawInput(raw.data.keyboard);
		else if (raw.header.dwType == RIM_TYPEMOUSE) m_mouse.OnRawInput(raw.data.mouse);
	}

	f32 InputManager::Evaluate(const InputBinding& binding) const noexcept
	{
		return std::visit([&](auto source) -> f32
		{
			using T = std::decay_t<decltype(source)>;
			if constexpr (std::is_same_v<T, KeyCode>)       return m_keyboard.IsDown(source)      ? binding.Scale : 0.0f;
			if constexpr (std::is_same_v<T, GamepadButton>) return m_gamepad.IsButtonDown(source) ? binding.Scale : 0.0f;
			if constexpr (std::is_same_v<T, GamepadAxis>)   return m_gamepad.GetAxis(source) * binding.Scale;
		}, binding.Source);
	}

	void InputManager::EvaluateActions()
	{
		ZoneScoped;

		for (size_t i = 0; i < m_config.Actions.size(); ++i)
		{
			ActionState& state = m_actions[i];
			state.DownPrev = state.Down;
			state.Value    = 0.0f;

			if (!m_gameFocused)
			{
				state.Down = false;
				continue;
			}

			// The strongest source wins_
			for (const InputBinding& binding : m_config.Actions[i].Bindings)
			{
				const f32 value = Evaluate(binding);
				if (std::abs(value) > std::abs(state.Value)) state.Value = value;
			}

			state.Down = std::abs(state.Value) >= kActionThreshold;
		}
	}

	f32 InputManager::GetAction(std::string_view name) const noexcept
	{
		for (size_t i = 0; i < m_config.Actions.size(); ++i)
		{
			if (m_config.Actions[i].Name == name) return m_actions[i].Value;
		}
		return 0.0f;
	}

	bool InputManager::IsActionDown(std::string_view name) const noexcept
	{
		for (size_t i = 0; i < m_config.Actions.size(); ++i)
		{
			if (m_config.Actions[i].Name == name) return m_actions[i].Down;
		}
		return false;
	}

	bool InputManager::IsActionPressed(std::string_view name) const noexcept
	{
		for (size_t i = 0; i < m_config.Actions.size(); ++i)
		{
			if (m_config.Actions[i].Name == name) return m_actions[i].Down && !m_actions[i].DownPrev;
		}
		return false;
	}

	bool InputManager::IsActionReleased(std::string_view name) const noexcept
	{
		for (size_t i = 0; i < m_config.Actions.size(); ++i)
		{
			if (m_config.Actions[i].Name == name) return !m_actions[i].Down && m_actions[i].DownPrev;
		}
		return false;
	}
}
