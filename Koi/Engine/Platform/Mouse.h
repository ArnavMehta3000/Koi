#pragma once
#include "Koi/Engine/Math/MathTypes.h"
#include "Koi/Engine/Platform/InputCodes.h"
#include <array>
#include <optional>

struct HWND__;
struct tagRAWMOUSE;

namespace koi
{
	struct WindowMessage;

	class Mouse : public NonCopyable, public NonMovable
	{
	public:
		[[nodiscard]] bool IsButtonDown(MouseButton btn)     const noexcept;
		[[nodiscard]] bool IsButtonPressed(MouseButton btn)  const noexcept;
		[[nodiscard]] bool IsButtonReleased(MouseButton btn) const noexcept;

		// Position is in client pixels; delta (raw counts) and scroll reset every frame
		[[nodiscard]] inline Vector2 GetPosition() const noexcept { return m_position; }
		[[nodiscard]] inline Vector2 GetDelta()    const noexcept { return m_delta; }
		[[nodiscard]] inline Vector2 GetScroll()   const noexcept { return m_scroll; }

		void SetCursorVisible(bool visible);
		void SetCursorLocked(bool locked);  // hidden and pinned to the client centre, deltas keep coming
		[[nodiscard]] inline bool IsCursorLocked() const noexcept { return m_cursorLocked; }

	private:
		friend class InputManager;

		void Init(HWND__* hwnd);
		void Shutdown();
		void BeginFrame() noexcept;
		void OnRawInput(const tagRAWMOUSE& raw);
		std::optional<i64> OnMessage(const WindowMessage& msg);
		void OnAppActivated(bool active);

		void ApplyCursorClip() const;
		[[nodiscard]] bool IsCursorOverClient() const noexcept;

	private:
		static constexpr u32 kMaxButton = static_cast<u32>(MouseButton::Count);

		HWND__* m_hwnd = nullptr;

		std::array<bool, kMaxButton> m_down{};
		std::array<bool, kMaxButton> m_downPrev{};

		Vector2 m_position{};
		Vector2 m_delta{};
		Vector2 m_scroll{};
		Vector2 m_scrollAccum{};
		Vector2 m_lastAbsolute{};  // absolute-mode devices (remote desktop, pens) report positions, not deltas
		bool    m_hasAbsolute = false;

		bool m_cursorHidden = false;
		bool m_cursorLocked = false;
	};
}
