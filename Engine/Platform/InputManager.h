#pragma once
#include "Core/Types.h"
#include "Core/MathTypes.h"
#include "Platform/InputCodes.h"
#include <array>

namespace koi
{
	class Window;

	class InputManager : public NonCopyable
	{
	public:
		InputManager();
		~InputManager();

		[[nodiscard]] static InputManager& Get() { return *s_instance; }

		// Called once by Engine after the main window is created.
		void Init(Window& window);

		void BeginFrame();

		// Returns true if ImGui wants to capture the input
		[[nodiscard]] bool ImGuiWantsCaptureMouse() const noexcept;
		[[nodiscard]] bool ImGuiWantsCaptureKeyboard() const noexcept;
		[[nodiscard]] bool ImGuiWantsCaptureInput() const noexcept;  // Either mouse or keyboard

		[[nodiscard]] bool IsKeyDown(KeyCode key)     const noexcept;
		[[nodiscard]] bool IsKeyPressed(KeyCode key)  const noexcept;
		[[nodiscard]] bool IsKeyReleased(KeyCode key) const noexcept;
		[[nodiscard]] bool IsModifierDown(Modifier mod) const noexcept;

		[[nodiscard]] bool IsMouseButtonDown(MouseButton btn)     const noexcept;
		[[nodiscard]] bool IsMouseButtonPressed(MouseButton btn)  const noexcept;
		[[nodiscard]] bool IsMouseButtonReleased(MouseButton btn) const noexcept;

		// Delta and scroll reset to zero at the start of each frame.
		[[nodiscard]] math::float2 GetMousePosition() const noexcept { return m_mousePos;    }
		[[nodiscard]] math::float2 GetMouseDelta()    const noexcept { return m_mouseDelta;  }
		[[nodiscard]] math::float2 GetScrollDelta()   const noexcept { return m_scrollDelta; }

		void SetCursorVisible(bool visible);
		void SetCursorLocked(bool locked);

	private:
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

		void* m_glfwWindow = nullptr;

		static inline InputManager* s_instance = nullptr;
	};
}
