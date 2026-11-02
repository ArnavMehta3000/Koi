#pragma once
#include "Koi/Engine/Platform/InputCodes.h"
#include <array>

struct tagRAWKEYBOARD;

namespace koi
{
	class Keyboard : public NonCopyable, public NonMovable
	{
	public:
		[[nodiscard]] bool IsDown(KeyCode key)     const noexcept;
		[[nodiscard]] bool IsPressed(KeyCode key)  const noexcept;
		[[nodiscard]] bool IsReleased(KeyCode key) const noexcept;
		[[nodiscard]] bool IsModifierDown(Modifier mod) const noexcept;

	private:
		friend class InputManager;

		void BeginFrame() noexcept { m_downPrev = m_down; }
		void OnRawInput(const tagRAWKEYBOARD& raw) noexcept;
		void ReleaseAll() noexcept;

		[[nodiscard]] static i32 IndexOf(KeyCode key) noexcept;

	private:
		static constexpr u32 kMaxKeyCode = 349;

		std::array<bool, kMaxKeyCode> m_down{};
		std::array<bool, kMaxKeyCode> m_downPrev{};
	};
}
