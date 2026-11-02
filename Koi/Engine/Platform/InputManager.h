#pragma once
#include "Koi/Engine/Math/MathTypes.h"
#include "Koi/Engine/Platform/InputActions.h"
#include "Koi/Engine/Platform/InputCodes.h"
#include "Koi/Engine/Runtime/Subsystems.h"
#include <array>
#include <optional>

namespace koi
{
	class Window;
	struct WindowMessage;

	class InputManager : public NonCopyable
	{
	public:
		InputManager();
		~InputManager();

		// Called once by Engine after the main window is created.
		void Init(const SubsystemInitContext& ctx);

		void BeginFrame();

		// Raw input data (used by editor)
		[[nodiscard]] bool IsKeyDown(KeyCode key)     const noexcept;
		[[nodiscard]] bool IsKeyPressed(KeyCode key)  const noexcept;
		[[nodiscard]] bool IsKeyReleased(KeyCode key) const noexcept;
		[[nodiscard]] bool IsModifierDown(Modifier mod) const noexcept;

		[[nodiscard]] bool IsMouseButtonDown(MouseButton btn)     const noexcept;
		[[nodiscard]] bool IsMouseButtonPressed(MouseButton btn)  const noexcept;
		[[nodiscard]] bool IsMouseButtonReleased(MouseButton btn) const noexcept;

		// Delta and scroll reset to zero at the start of each frame.
		[[nodiscard]] inline Vector2 GetMousePosition() const noexcept { return m_mousePos;    }
		[[nodiscard]] inline Vector2 GetMouseDelta()    const noexcept { return m_mouseDelta;  }
		[[nodiscard]] inline Vector2 GetScrollDelta()   const noexcept { return m_scrollDelta; }

		// First connected gamepad. Axes are deadzoned and in [-1, 1] (triggers [0, 1])
		[[nodiscard]] inline bool IsGamepadConnected() const noexcept { return m_gamepadConnected; }
		[[nodiscard]] bool IsGamepadButtonDown(GamepadButton btn)     const noexcept;
		[[nodiscard]] bool IsGamepadButtonPressed(GamepadButton btn)  const noexcept;
		[[nodiscard]] bool IsGamepadButtonReleased(GamepadButton btn) const noexcept;
		[[nodiscard]] f32  GetGamepadAxis(GamepadAxis axis) const noexcept;

		[[nodiscard]] f32  GetAction(std::string_view name) const noexcept;  // [-1, 1]
		[[nodiscard]] bool IsActionDown(std::string_view name)     const noexcept;
		[[nodiscard]] bool IsActionPressed(std::string_view name)  const noexcept;
		[[nodiscard]] bool IsActionReleased(std::string_view name) const noexcept;

		void SetCursorVisible(bool visible);
		void SetCursorLocked(bool locked);

		// Editor use only, always true in standalone builds
		inline void SetGameFocused(bool focused) noexcept { m_gameFocused = focused; }
		[[nodiscard]] inline bool IsGameFocused() const noexcept { return m_gameFocused; }

	private:
		std::optional<i64> OnMessage(const WindowMessage& msg);
		void OnRawInput(i64 handle);
		void ReleaseAll() noexcept;
		void UpdateModifiers() noexcept;
		void ApplyCursorClip() const;
		[[nodiscard]] bool IsCursorOverClient() const noexcept;

		void PollGamepad();
		void EvaluateActions();
		[[nodiscard]] f32 Evaluate(const InputBinding& binding) const noexcept;

		static constexpr u32 kMaxKeyCode       = 349;
		static constexpr u32 kMaxMouseButton   = static_cast<u32>(MouseButton::Count);
		static constexpr u32 kMaxGamepadButton = static_cast<u32>(GamepadButton::Count);
		static constexpr u32 kMaxGamepadAxis   = static_cast<u32>(GamepadAxis::Count);

		// Held when the action's magnitude crosses this: a stick nudge is not a press
		static constexpr f32 kActionThreshold = 0.5f;

		std::array<bool, kMaxKeyCode>     m_keyDown{};
		std::array<bool, kMaxKeyCode>     m_keyDownPrev{};
		std::array<bool, kMaxMouseButton> m_mouseDown{};
		std::array<bool, kMaxMouseButton> m_mouseDownPrev{};

		std::array<bool, kMaxGamepadButton> m_padDown{};
		std::array<bool, kMaxGamepadButton> m_padDownPrev{};
		std::array<f32,  kMaxGamepadAxis>   m_padAxes{};
		bool                                m_gamepadConnected = false;

		// Parallel to m_config.Actions
		struct ActionState { f32 Value = 0.0f; bool Down = false; bool DownPrev = false; };
		InputConfig              m_config;
		std::vector<ActionState> m_actions;

		EnumFlag<Modifier> m_modifiers{};

		bool m_gameFocused = true;

		Vector2 m_mousePos{};
		Vector2 m_mouseDelta{};
		Vector2 m_scrollDelta{};
		Vector2 m_scrollAccum{};
		Vector2 m_lastAbsolute{};  // absolute-mode devices (remote desktop, pens) report positions, not deltas
		bool    m_hasAbsolute = false;

		Window* m_window       = nullptr;
		u32     m_hookId       = 0;
		bool    m_cursorHidden = false;
		bool    m_cursorLocked = false;

		static inline InputManager* s_instance = nullptr;
	};
}
