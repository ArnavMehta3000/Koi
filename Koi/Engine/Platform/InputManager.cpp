#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

#include <algorithm>
#include <cmath>

namespace koi
{
	namespace
	{
		HWND HandleOf(const Window* window) { return window ? static_cast<HWND>(window->GetNativeHandle()) : nullptr; }
	}

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

			const RAWINPUTDEVICE devices[]
			{
				{ .usUsagePage = 0x01, .usUsage = 0x02, .dwFlags = RIDEV_REMOVE, .hwndTarget = nullptr },
				{ .usUsagePage = 0x01, .usUsage = 0x06, .dwFlags = RIDEV_REMOVE, .hwndTarget = nullptr },
			};
			::RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
			::ClipCursor(nullptr);
		}
		s_instance = nullptr;
	}

	void InputManager::Init(const SubsystemInitContext& ctx)
	{
		ZoneScoped;

		m_window = &ctx.MainWindow;
		const HWND hwnd = HandleOf(m_window);
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

		m_hookId = m_window->AddMessageHook([this](const WindowMessage& msg) { return OnMessage(msg); });

		if (POINT cursor{}; ::GetCursorPos(&cursor) && ::ScreenToClient(hwnd, &cursor))
		{
			m_mousePos = Vector2(static_cast<f32>(cursor.x), static_cast<f32>(cursor.y));
		}

		KOI_LOG(Input, Debug, "InputManager initialized");
	}

	void InputManager::BeginFrame()
	{
		ZoneScoped;

		m_keyDownPrev   = m_keyDown;
		m_mouseDownPrev = m_mouseDown;
		m_scrollDelta   = m_scrollAccum;
		m_scrollAccum   = {};
		m_mouseDelta    = {};

		PollGamepad();
		EvaluateActions();
	}

	void InputManager::PollGamepad()
	{
		ZoneScoped;

		m_padDownPrev = m_padDown;

		// [TODO]: GameInput
		m_gamepadConnected = false;
		m_padDown.fill(false);
		m_padAxes.fill(0.0f);
	}

	f32 InputManager::Evaluate(const InputBinding& binding) const noexcept
	{
		return std::visit([&](auto source) -> f32
		{
			using T = std::decay_t<decltype(source)>;
			if constexpr (std::is_same_v<T, KeyCode>)       return IsKeyDown(source)           ? binding.Scale : 0.0f;
			if constexpr (std::is_same_v<T, GamepadButton>) return IsGamepadButtonDown(source) ? binding.Scale : 0.0f;
			if constexpr (std::is_same_v<T, GamepadAxis>)   return GetGamepadAxis(source) * binding.Scale;
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

	bool InputManager::IsGamepadButtonDown(GamepadButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxGamepadButton && m_padDown[i];
	}

	bool InputManager::IsGamepadButtonPressed(GamepadButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxGamepadButton && m_padDown[i] && !m_padDownPrev[i];
	}

	bool InputManager::IsGamepadButtonReleased(GamepadButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxGamepadButton && !m_padDown[i] && m_padDownPrev[i];
	}

	f32 InputManager::GetGamepadAxis(GamepadAxis axis) const noexcept
	{
		const u32 i = static_cast<u32>(axis);
		return i < kMaxGamepadAxis ? m_padAxes[i] : 0.0f;
	}

	bool InputManager::IsKeyDown(KeyCode key) const noexcept
	{
		const i32 idx = static_cast<i32>(key);
		if (idx < 0 || idx >= static_cast<i32>(kMaxKeyCode)) return false;
		return m_keyDown[static_cast<u32>(idx)];
	}

	bool InputManager::IsKeyPressed(KeyCode key) const noexcept
	{
		const i32 idx = static_cast<i32>(key);
		if (idx < 0 || idx >= static_cast<i32>(kMaxKeyCode)) return false;
		const u32 i = static_cast<u32>(idx);
		return m_keyDown[i] && !m_keyDownPrev[i];
	}

	bool InputManager::IsKeyReleased(KeyCode key) const noexcept
	{
		const i32 idx = static_cast<i32>(key);
		if (idx < 0 || idx >= static_cast<i32>(kMaxKeyCode)) return false;
		const u32 i = static_cast<u32>(idx);
		return !m_keyDown[i] && m_keyDownPrev[i];
	}

	bool InputManager::IsModifierDown(Modifier mod) const noexcept { return m_modifiers.Has(mod); }

	bool InputManager::IsMouseButtonDown(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxMouseButton && m_mouseDown[i];
	}

	bool InputManager::IsMouseButtonPressed(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxMouseButton && m_mouseDown[i] && !m_mouseDownPrev[i];
	}

	bool InputManager::IsMouseButtonReleased(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxMouseButton && !m_mouseDown[i] && m_mouseDownPrev[i];
	}

	void InputManager::SetCursorVisible(bool visible)
	{
		m_cursorHidden = !visible;
		if (m_cursorHidden && IsCursorOverClient()) ::SetCursor(nullptr);
	}

	// Locked = hidden and pinned to the client centre; deltas keep flowing from raw input
	void InputManager::SetCursorLocked(bool locked)
	{
		m_cursorLocked = locked;
		SetCursorVisible(!locked);

		if (locked) ApplyCursorClip();
		else ::ClipCursor(nullptr);
	}

	void InputManager::ApplyCursorClip() const
	{
		const HWND hwnd = HandleOf(m_window);
		RECT client{};
		if (!hwnd || !::GetClientRect(hwnd, &client)) return;

		POINT centre{ (client.left + client.right) / 2, (client.top + client.bottom) / 2 };
		::ClientToScreen(hwnd, &centre);
		const RECT clip{ centre.x, centre.y, centre.x + 1, centre.y + 1 };
		::ClipCursor(&clip);
	}

	bool InputManager::IsCursorOverClient() const noexcept
	{
		if (m_cursorLocked) return true;

		const HWND hwnd = HandleOf(m_window);
		POINT cursor{};
		if (!hwnd || !::GetCursorPos(&cursor) || ::WindowFromPoint(cursor) != hwnd) return false;

		RECT client{};
		::GetClientRect(hwnd, &client);
		::ScreenToClient(hwnd, &cursor);
		return ::PtInRect(&client, cursor);
	}

	std::optional<i64> InputManager::OnMessage(const WindowMessage& msg)
	{
		switch (msg.Msg)
		{
		case WM_INPUT:
			OnRawInput(msg.LParam);
			break;  // DefWindowProc still has to clean up after WM_INPUT

		case WM_MOUSEMOVE:
			m_mousePos = Vector2(static_cast<f32>(static_cast<i16>(LOWORD(msg.LParam))), static_cast<f32>(static_cast<i16>(HIWORD(msg.LParam))));
			break;

		case WM_SETCURSOR:
			if (m_cursorHidden && LOWORD(msg.LParam) == HTCLIENT)
			{
				::SetCursor(nullptr);
				return TRUE;
			}
			break;

		case WM_ACTIVATEAPP:
			// Key-ups that land while another app is in front never reach us
			if (msg.WParam == FALSE)
			{
				ReleaseAll();
				::ClipCursor(nullptr);
			}
			else if (m_cursorLocked)
			{
				ApplyCursorClip();
			}
			break;

		case WM_MOVE:
		case WM_SIZE:
			if (m_cursorLocked) ApplyCursorClip();
			break;
		}
		return std::nullopt;
	}

	void InputManager::OnRawInput(i64 handle)
	{
		RAWINPUT raw{};
		UINT size = sizeof(raw);
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(handle), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
		{
			return;
		}

		if (raw.header.dwType == RIM_TYPEKEYBOARD)
		{
			const RAWKEYBOARD& kb = raw.data.keyboard;
			if (kb.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || kb.VKey == 0xFF) return;  // 0xFF: fake key inside an escape sequence

			KeyCode key = KeyCode::Pause;  // E1 1D 45, the only E1 sequence
			if (kb.VKey != VK_PAUSE)
			{
				const u16 scancode = kb.MakeCode != 0
					? static_cast<u16>(kb.MakeCode | ((kb.Flags & RI_KEY_E0) ? 0xE000 : 0))
					: static_cast<u16>(::MapVirtualKeyW(kb.VKey, MAPVK_VK_TO_VSC_EX));
				key = utils::KeyCodeFromScancode(scancode);
			}

			const i32 index = static_cast<i32>(key);
			if (index < 0 || index >= static_cast<i32>(kMaxKeyCode)) return;

			m_keyDown[static_cast<u32>(index)] = (kb.Flags & RI_KEY_BREAK) == 0;
			UpdateModifiers();
			return;
		}

		if (raw.header.dwType != RIM_TYPEMOUSE) return;
		const RAWMOUSE& mouse = raw.data.mouse;

		if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
		{
			const bool virtualDesk = mouse.usFlags & MOUSE_VIRTUAL_DESKTOP;
			const Vector2 extent(
				static_cast<f32>(::GetSystemMetrics(virtualDesk ? SM_CXVIRTUALSCREEN : SM_CXSCREEN)),
				static_cast<f32>(::GetSystemMetrics(virtualDesk ? SM_CYVIRTUALSCREEN : SM_CYSCREEN)));
			const Vector2 position = Vector2(static_cast<f32>(mouse.lLastX), static_cast<f32>(mouse.lLastY)) / 65535.0f * extent;

			if (m_hasAbsolute) m_mouseDelta = m_mouseDelta + (position - m_lastAbsolute);
			m_lastAbsolute = position;
			m_hasAbsolute  = true;
		}
		else
		{
			m_mouseDelta = m_mouseDelta + Vector2(static_cast<f32>(mouse.lLastX), static_cast<f32>(mouse.lLastY));
		}

		const USHORT flags = mouse.usButtonFlags;
		if (flags == 0) return;

		// Presses only count over our own client area, so title bar drags and floating editor windows stay out
		const bool overClient = IsCursorOverClient();
		for (u32 i = 0; i < 5; ++i)
		{
			if (overClient && (flags & (RI_MOUSE_BUTTON_1_DOWN << (2 * i)))) m_mouseDown[i] = true;
			if (flags & (RI_MOUSE_BUTTON_1_UP << (2 * i)))                  m_mouseDown[i] = false;
		}

		if (overClient)
		{
			const f32 wheel = static_cast<f32>(static_cast<SHORT>(mouse.usButtonData)) / WHEEL_DELTA;
			if (flags & RI_MOUSE_WHEEL)  m_scrollAccum.y += wheel;
			if (flags & RI_MOUSE_HWHEEL) m_scrollAccum.x -= wheel;
		}
		UpdateModifiers();
	}

	void InputManager::UpdateModifiers() noexcept
	{
		const auto down = [this](KeyCode a, KeyCode b) { return m_keyDown[static_cast<u32>(a)] || m_keyDown[static_cast<u32>(b)]; };

		u8 mods = 0;
		if (down(KeyCode::LeftShift,   KeyCode::RightShift))   mods |= static_cast<u8>(Modifier::Shift);
		if (down(KeyCode::LeftControl, KeyCode::RightControl)) mods |= static_cast<u8>(Modifier::Control);
		if (down(KeyCode::LeftAlt,     KeyCode::RightAlt))     mods |= static_cast<u8>(Modifier::Alt);
		if (down(KeyCode::LeftSuper,   KeyCode::RightSuper))   mods |= static_cast<u8>(Modifier::Super);
		if (::GetKeyState(VK_CAPITAL) & 1) mods |= static_cast<u8>(Modifier::CapsLock);
		if (::GetKeyState(VK_NUMLOCK) & 1) mods |= static_cast<u8>(Modifier::NumLock);
		m_modifiers = EnumFlag<Modifier>(mods);
	}

	void InputManager::ReleaseAll() noexcept
	{
		m_keyDown.fill(false);
		m_mouseDown.fill(false);
		m_hasAbsolute = false;
		UpdateModifiers();
	}
}
