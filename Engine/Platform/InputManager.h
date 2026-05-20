#pragma once
#include "Core/Types.h"
#include "Core/MathTypes.h"
#include "Platform/InputCodes.h"
#include "Platform/WindowManager.h"
#include <array>
#include <vector>

namespace koi
{
	class Window;

	// Per-window input state
	// Returned pointers are stable as long as no windows are registered or unregistered.
	class WindowInput
	{
		friend class InputManager;
	public:
		[[nodiscard]] bool IsKeyDown(KeyCode key)     const noexcept;
		[[nodiscard]] bool IsKeyPressed(KeyCode key)  const noexcept;
		[[nodiscard]] bool IsKeyReleased(KeyCode key) const noexcept;
		[[nodiscard]] bool IsModifierDown(Modifier mod) const noexcept;

		[[nodiscard]] bool IsMouseButtonDown(MouseButton btn)     const noexcept;
		[[nodiscard]] bool IsMouseButtonPressed(MouseButton btn)  const noexcept;
		[[nodiscard]] bool IsMouseButtonReleased(MouseButton btn) const noexcept;

		// Delta and scroll reset to zero at the start of each frame
		[[nodiscard]] math::float2 GetMousePosition() const noexcept { return m_mousePos;    }
		[[nodiscard]] math::float2 GetMouseDelta()    const noexcept { return m_mouseDelta;  }
		[[nodiscard]] math::float2 GetScrollDelta()   const noexcept { return m_scrollDelta; }

	private:
		void BeginFrame() noexcept;
		void OnKey(int key, int action, int mods) noexcept;
		void OnMouseButton(int btn, int action, int mods) noexcept;
		void OnCursorPos(double x, double y) noexcept;
		void OnScroll(double dx, double dy) noexcept;

		static constexpr u32 kMaxKeyCode     = 349;
		static constexpr u32 kMaxMouseButton = static_cast<u32>(MouseButton::Count);

		std::array<bool, kMaxKeyCode>     m_keyDown{};
		std::array<bool, kMaxKeyCode>     m_keyDownPrev{};
		std::array<bool, kMaxMouseButton> m_mouseDown{};
		std::array<bool, kMaxMouseButton> m_mouseDownPrev{};

		EnumFlag<Modifier> m_modifiers{};

		math::float2 m_mousePos{};
		math::float2 m_mouseDelta{};
		math::float2 m_scrollDelta{};
		math::float2 m_scrollAccum{};
	};


	class InputManager : public NonCopyable
	{
	public:
		InputManager();
		~InputManager();

		[[nodiscard]] static InputManager& Get() { return *s_instance; }

		void RegisterWindow(WindowHandle handle, Window& window);
		void UnregisterWindow(WindowHandle handle);
		void BeginFrame();

		// Returns nullptr if the handle is not registered
		[[nodiscard]] const WindowInput* GetWindowInput(WindowHandle handle) const noexcept;
		[[nodiscard]] WindowInput*       GetWindowInput(WindowHandle handle)       noexcept;

		void SetCursorVisible(WindowHandle handle, bool visible);
		void SetCursorLocked(WindowHandle handle, bool locked);

	private:
		void DispatchKey(void* glfwWnd, int key, int action, int mods) noexcept;
		void DispatchMouseButton(void* glfwWnd, int btn, int action, int mods) noexcept;
		void DispatchCursorPos(void* glfwWnd, double x, double y) noexcept;
		void DispatchScroll(void* glfwWnd, double dx, double dy) noexcept;

		struct Entry
		{
			WindowHandle handle;
			void*        glfwWindow = nullptr; // GLFWwindow* stored opaque to keep GLFW out of this header
			WindowInput  input;
		};

		Entry*       FindByGLFW(void* glfwWnd) noexcept;
		Entry*       FindByHandle(WindowHandle handle) noexcept;
		const Entry* FindByHandle(WindowHandle handle) const noexcept;

		std::vector<Entry> m_windows;

		static inline InputManager* s_instance = nullptr;
	};
}
