#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Diagnostics/Profiling.h"

#ifndef GLFW_INCLUDE_NONE
	#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace koi
{
	InputManager::InputManager()
	{
		assert(s_instance == nullptr && "InputManager already exists");
		s_instance = this;
	}

	InputManager::~InputManager()
	{
		if (m_glfwWindow)
		{
			auto* w = static_cast<GLFWwindow*>(m_glfwWindow);
			::glfwSetKeyCallback(w, nullptr);
			::glfwSetMouseButtonCallback(w, nullptr);
			::glfwSetCursorPosCallback(w, nullptr);
			::glfwSetScrollCallback(w, nullptr);
		}
		s_instance = nullptr;
	}

	void InputManager::Init(const SubsystemInitContext& ctx)
	{
		ZoneScoped;

		Window& window = ctx.MainWindow;
		GLFWwindow* glfwWnd = window.GetGLFWWindow();
		assert(glfwWnd && "Window has a null GLFWwindow");
		m_glfwWindow = glfwWnd;

		m_config = ctx.Input;
		m_actions.assign(m_config.Actions.size(), ActionState{});

		// Seed cursor position so the first frame's delta is zero.
		f64 x, y;
		::glfwGetCursorPos(glfwWnd, &x, &y);
		m_mousePos = { static_cast<f32>(x), static_cast<f32>(y) };

		::glfwSetKeyCallback(glfwWnd,
			[](GLFWwindow*, i32 key, i32 /*scancode*/, i32 action, i32 mods) noexcept
			{ s_instance->OnKey(key, action, mods); });

		::glfwSetMouseButtonCallback(glfwWnd,
			[](GLFWwindow*, i32 btn, i32 action, i32 mods) noexcept
			{ s_instance->OnMouseButton(btn, action, mods); });

		::glfwSetCursorPosCallback(glfwWnd,
			[](GLFWwindow*, f64 x, f64 y) noexcept
			{ s_instance->OnCursorPos(x, y); });

		::glfwSetScrollCallback(glfwWnd,
			[](GLFWwindow*, f64 dx, f64 dy) noexcept
			{ s_instance->OnScroll(dx, dy); });

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

		GLFWgamepadstate state{};
		m_gamepadConnected = ::glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && ::glfwGetGamepadState(GLFW_JOYSTICK_1, &state);
		if (!m_gamepadConnected)
		{
			m_padDown.fill(false);
			m_padAxes.fill(0.0f);
			return;
		}

		for (u32 i = 0; i < kMaxGamepadButton; ++i)
		{
			m_padDown[i] = state.buttons[i] == GLFW_PRESS;
		}

		// Sticks: radial deadzone, rescaled so the edge of the zone reads as zero
		const auto stick = [&](GamepadAxis x, GamepadAxis y)
		{
			Vector2 v(state.axes[static_cast<u32>(x)], state.axes[static_cast<u32>(y)]);
			const f32 length   = v.Length();
			const f32 deadzone = m_config.Gamepad.StickDeadzone;

			if (length <= deadzone) v = Vector2::Zero;
			else v = v * ((length - deadzone) / ((1.0f - deadzone) * length));

			m_padAxes[static_cast<u32>(x)] = std::clamp(v.x, -1.0f, 1.0f);
			m_padAxes[static_cast<u32>(y)] = std::clamp(v.y, -1.0f, 1.0f);
		};
		stick(GamepadAxis::LeftX,  GamepadAxis::LeftY);
		stick(GamepadAxis::RightX, GamepadAxis::RightY);

		// Triggers: GLFW reports [-1, 1] with -1 released; map to [0, 1] past the deadzone
		const auto trigger = [&](GamepadAxis axis)
		{
			const f32 raw      = (state.axes[static_cast<u32>(axis)] + 1.0f) * 0.5f;
			const f32 deadzone = m_config.Gamepad.TriggerDeadzone;
			m_padAxes[static_cast<u32>(axis)] = raw <= deadzone ? 0.0f : (raw - deadzone) / (1.0f - deadzone);
		};
		trigger(GamepadAxis::LeftTrigger);
		trigger(GamepadAxis::RightTrigger);
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
		if (m_glfwWindow)
			::glfwSetInputMode(static_cast<GLFWwindow*>(m_glfwWindow), GLFW_CURSOR,
				visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
	}

	void InputManager::SetCursorLocked(bool locked)
	{
		if (m_glfwWindow)
			::glfwSetInputMode(static_cast<GLFWwindow*>(m_glfwWindow), GLFW_CURSOR,
				locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}

	void InputManager::OnKey(i32 key, i32 action, i32 mods) noexcept
	{
		if (key < 0 || key >= static_cast<i32>(kMaxKeyCode)) return;
		if (action == GLFW_REPEAT) return;
		m_keyDown[static_cast<u32>(key)] = (action == GLFW_PRESS);
		m_modifiers = EnumFlag<Modifier>(static_cast<u8>(mods));
	}

	void InputManager::OnMouseButton(i32 btn, i32 action, i32 mods) noexcept
	{
		if (btn < 0 || btn >= static_cast<i32>(kMaxMouseButton)) return;
		m_mouseDown[static_cast<u32>(btn)] = (action == GLFW_PRESS);
		m_modifiers = EnumFlag<Modifier>(static_cast<u8>(mods));
	}

	void InputManager::OnCursorPos(f64 x, f64 y) noexcept
	{
		const Vector2 newPos = { static_cast<f32>(x), static_cast<f32>(y) };
		m_mouseDelta         = m_mouseDelta + (newPos - m_mousePos);
		m_mousePos           = newPos;
	}

	void InputManager::OnScroll(f64 dx, f64 dy) noexcept
	{
		m_scrollAccum = m_scrollAccum + Vector2(static_cast<f32>(dx), static_cast<f32>(dy));
	}
}
