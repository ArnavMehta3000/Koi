#pragma once
#include "Koi/Engine/Platform/Gamepad.h"
#include "Koi/Engine/Platform/InputActions.h"
#include "Koi/Engine/Platform/Keyboard.h"
#include "Koi/Engine/Platform/Mouse.h"
#include "Koi/Engine/Runtime/Subsystems.h"
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

		// Devices are never gated by game focus; actions are
		[[nodiscard]] inline Keyboard&       GetKeyboard()       noexcept { return m_keyboard; }
		[[nodiscard]] inline const Keyboard& GetKeyboard() const noexcept { return m_keyboard; }
		[[nodiscard]] inline Mouse&          GetMouse()          noexcept { return m_mouse; }
		[[nodiscard]] inline const Mouse&    GetMouse()    const noexcept { return m_mouse; }
		[[nodiscard]] inline Gamepad&        GetGamepad()        noexcept { return m_gamepad; }
		[[nodiscard]] inline const Gamepad&  GetGamepad()  const noexcept { return m_gamepad; }

		[[nodiscard]] f32  GetAction(std::string_view name) const noexcept;  // [-1, 1]
		[[nodiscard]] bool IsActionDown(std::string_view name)     const noexcept;
		[[nodiscard]] bool IsActionPressed(std::string_view name)  const noexcept;
		[[nodiscard]] bool IsActionReleased(std::string_view name) const noexcept;

		// Editor use only, always true in standalone builds
		inline void SetGameFocused(bool focused) noexcept { m_gameFocused = focused; }
		[[nodiscard]] inline bool IsGameFocused() const noexcept { return m_gameFocused; }

	private:
		std::optional<i64> OnMessage(const WindowMessage& msg);
		void OnRawInput(i64 handle);

		void EvaluateActions();
		[[nodiscard]] f32 Evaluate(const InputBinding& binding) const noexcept;

		// Held when the action's magnitude crosses this: a stick nudge is not a press
		static constexpr f32 kActionThreshold = 0.5f;

		Keyboard m_keyboard;
		Mouse    m_mouse;
		Gamepad  m_gamepad;

		// Parallel to m_config.Actions
		struct ActionState { f32 Value = 0.0f; bool Down = false; bool DownPrev = false; };
		InputConfig              m_config;
		std::vector<ActionState> m_actions;

		bool    m_gameFocused = true;
		bool    m_appActive   = true;
		Window* m_window      = nullptr;
		u32     m_hookId      = 0;

		static inline InputManager* s_instance = nullptr;
	};
}
