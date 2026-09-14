#include "Koi/Engine/Platform/InputManager.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"

#ifndef GLFW_INCLUDE_NONE
	#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <cassert>

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
		Window& window = ctx.MainWindow;
		GLFWwindow* glfwWnd = window.GetGLFWWindow();
		assert(glfwWnd && "Window has a null GLFWwindow");
		m_glfwWindow = glfwWnd;

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

		KOI_LOG_DEBUG("InputManager initialized");
	}

	void InputManager::BeginFrame()
	{
		m_keyDownPrev   = m_keyDown;
		m_mouseDownPrev = m_mouseDown;
		m_scrollDelta   = m_scrollAccum;
		m_scrollAccum   = {};
		m_mouseDelta    = {};
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
