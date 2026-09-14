#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Platform/InputCodes.h"
#include "Koi/Engine/Runtime/Subsystems.h"
#include <array>

namespace koi
{
	class Window;

	class InputManager : public NonCopyable
	{
	public:
		InputManager();
		~InputManager();

		// Called once by Engine after the main window is created.
		void Init(const SubsystemInitContext& ctx);

		void BeginFrame();

		[[nodiscard]] bool IsKeyDown(KeyCode key)     const noexcept;
		[[nodiscard]] bool IsKeyPressed(KeyCode key)  const noexcept;
		[[nodiscard]] bool IsKeyReleased(KeyCode key) const noexcept;
		[[nodiscard]] bool IsModifierDown(Modifier mod) const noexcept;

		[[nodiscard]] bool IsMouseButtonDown(MouseButton btn)     const noexcept;
		[[nodiscard]] bool IsMouseButtonPressed(MouseButton btn)  const noexcept;
		[[nodiscard]] bool IsMouseButtonReleased(MouseButton btn) const noexcept;

		// Delta and scroll reset to zero at the start of each frame.
		[[nodiscard]] Vector2 GetMousePosition() const noexcept { return m_mousePos;    }
		[[nodiscard]] Vector2 GetMouseDelta()    const noexcept { return m_mouseDelta;  }
		[[nodiscard]] Vector2 GetScrollDelta()   const noexcept { return m_scrollDelta; }

		void SetCursorVisible(bool visible);
		void SetCursorLocked(bool locked);

		// Whether the game (as opposed to some other editor panel) should see input this frame.
		// Standalone builds never touch this - it stays true, since there is no other panel to lose
		// focus to. The editor's viewport panel drives it from its own hover/capture state
		void SetGameFocused(bool focused) noexcept { m_gameFocused = focused; }
		[[nodiscard]] bool IsGameFocused() const noexcept { return m_gameFocused; }

	private:
		void OnKey(i32 key, i32 action, i32 mods) noexcept;
		void OnMouseButton(i32 btn, i32 action, i32 mods) noexcept;
		void OnCursorPos(f64 x, f64 y) noexcept;
		void OnScroll(f64 dx, f64 dy) noexcept;

		static constexpr u32 kMaxKeyCode     = 349;
		static constexpr u32 kMaxMouseButton = static_cast<u32>(MouseButton::Count);

		std::array<bool, kMaxKeyCode>     m_keyDown{};
		std::array<bool, kMaxKeyCode>     m_keyDownPrev{};
		std::array<bool, kMaxMouseButton> m_mouseDown{};
		std::array<bool, kMaxMouseButton> m_mouseDownPrev{};

		EnumFlag<Modifier> m_modifiers{};

		bool m_gameFocused = true;

		Vector2 m_mousePos{};
		Vector2 m_mouseDelta{};
		Vector2 m_scrollDelta{};
		Vector2 m_scrollAccum{};

		void* m_glfwWindow = nullptr;

		static inline InputManager* s_instance = nullptr;
	};
}
