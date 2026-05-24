#include "Platform/InputManager.h"
#include "Platform/Window.h"
#include "Memory/MemoryTracker.h"
#include "Core/Logger.h"
#include <cassert>
#include <algorithm>

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace koi
{
	void WindowInput::BeginFrame() noexcept
	{
		KOI_MEM_TAG_SCOPE(MemTag::Input);
		m_keyDownPrev   = m_keyDown;
		m_mouseDownPrev = m_mouseDown;
		m_scrollDelta   = m_scrollAccum;
		m_scrollAccum   = {};
		m_mouseDelta    = {};
	}

	void WindowInput::OnKey(int key, int action, int mods) noexcept
	{
		if (key < 0 || key >= static_cast<i32>(kMaxKeyCode)) return;
		if (action == GLFW_REPEAT) return;

		m_keyDown[static_cast<u32>(key)] = (action == GLFW_PRESS);
		m_modifiers = EnumFlag<Modifier>(static_cast<u8>(mods));
	}

	void WindowInput::OnMouseButton(int btn, int action, int mods) noexcept
	{
		if (btn < 0 || btn >= static_cast<i32>(kMaxMouseButton)) return;

		m_mouseDown[static_cast<u32>(btn)] = (action == GLFW_PRESS);
		m_modifiers = EnumFlag<Modifier>(static_cast<u8>(mods));
	}

	void WindowInput::OnCursorPos(f64 x, f64 y) noexcept
	{
		const math::float2 newPos = { static_cast<f32>(x), static_cast<f32>(y) };
		m_mouseDelta              = m_mouseDelta + (newPos - m_mousePos);
		m_mousePos                = newPos;
	}

	void WindowInput::OnScroll(f64 dx, f64 dy) noexcept
	{
		m_scrollAccum = m_scrollAccum + math::float2(static_cast<f32>(dx), static_cast<f32>(dy));
	}

	bool WindowInput::IsKeyDown(KeyCode key) const noexcept
	{
		const i32 idx = static_cast<i32>(key);
		if (idx < 0 || idx >= static_cast<i32>(kMaxKeyCode)) return false;
		return m_keyDown[static_cast<u32>(idx)];
	}

	bool WindowInput::IsKeyPressed(KeyCode key) const noexcept
	{
		const i32 idx = static_cast<i32>(key);
		if (idx < 0 || idx >= static_cast<i32>(kMaxKeyCode)) return false;
		const u32 i = static_cast<u32>(idx);
		return m_keyDown[i] && !m_keyDownPrev[i];
	}

	bool WindowInput::IsKeyReleased(KeyCode key) const noexcept
	{
		const i32 idx = static_cast<i32>(key);
		if (idx < 0 || idx >= static_cast<i32>(kMaxKeyCode)) return false;
		const u32 i = static_cast<u32>(idx);
		return !m_keyDown[i] && m_keyDownPrev[i];
	}

	bool WindowInput::IsModifierDown(Modifier mod) const noexcept
	{
		return m_modifiers.Has(mod);
	}

	bool WindowInput::IsMouseButtonDown(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		if (i >= kMaxMouseButton) return false;
		return m_mouseDown[i];
	}

	bool WindowInput::IsMouseButtonPressed(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		if (i >= kMaxMouseButton) return false;
		return m_mouseDown[i] && !m_mouseDownPrev[i];
	}

	bool WindowInput::IsMouseButtonReleased(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		if (i >= kMaxMouseButton) return false;
		return !m_mouseDown[i] && m_mouseDownPrev[i];
	}


	InputManager::InputManager()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Input);
		assert(s_instance == nullptr && "InputManager already exists");
		m_windows.reserve(8);
		s_instance = this;
	}

	InputManager::~InputManager()
	{
		s_instance = nullptr;
	}

	void InputManager::RegisterWindow(WindowHandle handle, Window& window)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Input);
		assert(!FindByHandle(handle) && "Window is already registered with InputManager");

		GLFWwindow* glfwWnd = window.GetGLFWWindow();
		assert(glfwWnd && "Window has a null GLFWwindow");

		m_windows.push_back({ handle, static_cast<void*>(glfwWnd), WindowInput{} });

		// Seed cursor position so the first frame's delta is zero
		f64 x, y;
		::glfwGetCursorPos(glfwWnd, &x, &y);
		m_windows.back().input.m_mousePos = { static_cast<f32>(x), static_cast<f32>(y) };

		// Captureless lambdas: defined inside an InputManager member function so they
		// have access to private Dispatch* methods via InputManager::Get().
		::glfwSetKeyCallback(glfwWnd,
			[](GLFWwindow* w, int key, int /*scancode*/, int action, int mods) noexcept
			{ InputManager::Get().DispatchKey(w, key, action, mods); });

		::glfwSetMouseButtonCallback(glfwWnd,
			[](GLFWwindow* w, int btn, int action, int mods) noexcept
			{ InputManager::Get().DispatchMouseButton(w, btn, action, mods); });

		::glfwSetCursorPosCallback(glfwWnd,
			[](GLFWwindow* w, f64 x, f64 y) noexcept
			{ InputManager::Get().DispatchCursorPos(w, x, y); });

		::glfwSetScrollCallback(glfwWnd,
			[](GLFWwindow* w, f64 dx, f64 dy) noexcept
			{ InputManager::Get().DispatchScroll(w, dx, dy); });

		KOI_LOG_TRACE("InputManager: registered window (handle={})", handle.GetId());
	}

	void InputManager::UnregisterWindow(WindowHandle handle)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Input);
		auto it = std::ranges::find_if(m_windows,
			[handle](const Entry& e) { return e.handle == handle; });

		if (it == m_windows.end()) return;

		auto* glfwWnd = static_cast<GLFWwindow*>(it->glfwWindow);
		::glfwSetKeyCallback(glfwWnd, nullptr);
		::glfwSetMouseButtonCallback(glfwWnd, nullptr);
		::glfwSetCursorPosCallback(glfwWnd, nullptr);
		::glfwSetScrollCallback(glfwWnd, nullptr);

		m_windows.erase(it);
		KOI_LOG_TRACE("InputManager: unregistered window (handle={})", handle.GetId());
	}

	void InputManager::BeginFrame()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Input);
		for (Entry& e : m_windows)
		{
			e.input.BeginFrame();
		}
	}

	const WindowInput* InputManager::GetWindowInput(WindowHandle handle) const noexcept
	{
		if (const Entry* e = FindByHandle(handle)) return &e->input;
		return nullptr;
	}

	WindowInput* InputManager::GetWindowInput(WindowHandle handle) noexcept
	{
		if (Entry* e = FindByHandle(handle)) return &e->input;
		return nullptr;
	}

	void InputManager::SetCursorVisible(WindowHandle handle, bool visible)
	{
		if (Entry* e = FindByHandle(handle))
			::glfwSetInputMode(static_cast<GLFWwindow*>(e->glfwWindow), GLFW_CURSOR,
				visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
	}

	void InputManager::SetCursorLocked(WindowHandle handle, bool locked)
	{
		if (Entry* e = FindByHandle(handle))
			::glfwSetInputMode(static_cast<GLFWwindow*>(e->glfwWindow), GLFW_CURSOR,
				locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	}

	// --- Dispatch (called from GLFW callbacks above) ---

	void InputManager::DispatchKey(void* glfwWnd, int key, int action, int mods) noexcept
	{
		if (Entry* e = FindByGLFW(glfwWnd)) e->input.OnKey(key, action, mods);
	}

	void InputManager::DispatchMouseButton(void* glfwWnd, int btn, int action, int mods) noexcept
	{
		if (Entry* e = FindByGLFW(glfwWnd)) e->input.OnMouseButton(btn, action, mods);
	}

	void InputManager::DispatchCursorPos(void* glfwWnd, f64 x, f64 y) noexcept
	{
		if (Entry* e = FindByGLFW(glfwWnd)) e->input.OnCursorPos(x, y);
	}

	void InputManager::DispatchScroll(void* glfwWnd, f64 dx, f64 dy) noexcept
	{
		if (Entry* e = FindByGLFW(glfwWnd)) e->input.OnScroll(dx, dy);
	}

	// --- Lookup helpers ---

	InputManager::Entry* InputManager::FindByGLFW(void* glfwWnd) noexcept
	{
		for (Entry& e : m_windows)
		{
			if (e.glfwWindow == glfwWnd) return &e;
		}
		return nullptr;
	}

	InputManager::Entry* InputManager::FindByHandle(WindowHandle handle) noexcept
	{
		for (Entry& e : m_windows)
		{
			if (e.handle == handle) return &e;
		}
		return nullptr;
	}

	const InputManager::Entry* InputManager::FindByHandle(WindowHandle handle) const noexcept
	{
		for (const Entry& e : m_windows)
		{
			if (e.handle == handle) return &e;
		}
		return nullptr;
	}
}
