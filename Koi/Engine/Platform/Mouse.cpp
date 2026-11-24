#include "Koi/Engine/Platform/Mouse.h"
#include "Koi/Engine/Core/Win32.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"

namespace koi
{
	bool Mouse::IsButtonDown(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxButton && m_down[i];
	}

	bool Mouse::IsButtonPressed(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxButton && m_down[i] && !m_downPrev[i];
	}

	bool Mouse::IsButtonReleased(MouseButton btn) const noexcept
	{
		const u32 i = static_cast<u32>(btn);
		return i < kMaxButton && !m_down[i] && m_downPrev[i];
	}

	void Mouse::SetCursorVisible(bool visible)
	{
		m_cursorHidden = !visible;
		if (m_cursorHidden && IsCursorOverClient()) ::SetCursor(nullptr);
	}

	void Mouse::SetCursorLocked(bool locked)
	{
		if (locked != m_cursorLocked)
		{
			KOI_LOG(Input, Trace, "Cursor {}", locked ? "locked" : "released");
		}

		m_cursorLocked = locked;
		SetCursorVisible(!locked);

		if (locked) ApplyCursorClip();
		else ::ClipCursor(nullptr);
	}

	void Mouse::Init(HWND__* hwnd)
	{
		m_hwnd = hwnd;
		if (POINT cursor{}; ::GetCursorPos(&cursor) && ::ScreenToClient(m_hwnd, &cursor))
		{
			m_position = Vector2(static_cast<f32>(cursor.x), static_cast<f32>(cursor.y));
		}
	}

	void Mouse::Shutdown()
	{
		if (m_cursorLocked) ::ClipCursor(nullptr);
		m_hwnd = nullptr;
	}

	void Mouse::BeginFrame() noexcept
	{
		m_downPrev    = m_down;
		m_delta       = Vector2::Zero;
		m_scroll      = m_scrollAccum;
		m_scrollAccum = Vector2::Zero;
	}

	void Mouse::OnRawInput(const RAWMOUSE& raw)
	{
		if (raw.usFlags & MOUSE_MOVE_ABSOLUTE)
		{
			const bool virtualDesk = raw.usFlags & MOUSE_VIRTUAL_DESKTOP;
			const Vector2 extent(
				static_cast<f32>(::GetSystemMetrics(virtualDesk ? SM_CXVIRTUALSCREEN : SM_CXSCREEN)),
				static_cast<f32>(::GetSystemMetrics(virtualDesk ? SM_CYVIRTUALSCREEN : SM_CYSCREEN)));
			const Vector2 position = Vector2(static_cast<f32>(raw.lLastX), static_cast<f32>(raw.lLastY)) / 65535.0f * extent;

			if (m_hasAbsolute) m_delta = m_delta + (position - m_lastAbsolute);
			m_lastAbsolute = position;
			m_hasAbsolute  = true;
		}
		else
		{
			m_delta = m_delta + Vector2(static_cast<f32>(raw.lLastX), static_cast<f32>(raw.lLastY));
		}

		const USHORT flags = raw.usButtonFlags;
		if (flags == 0) return;

		// Presses only count over our own client area, so title bar drags and floating editor windows stay out
		const bool overClient = IsCursorOverClient();
		for (u32 i = 0; i < 5; ++i)  // raw input reports five buttons
		{
			if (overClient && (flags & (RI_MOUSE_BUTTON_1_DOWN << (2 * i)))) m_down[i] = true;
			if (flags & (RI_MOUSE_BUTTON_1_UP << (2 * i)))                   m_down[i] = false;
		}

		if (overClient)
		{
			const f32 wheel = static_cast<f32>(static_cast<SHORT>(raw.usButtonData)) / WHEEL_DELTA;
			if (flags & RI_MOUSE_WHEEL)  m_scrollAccum.y += wheel;
			if (flags & RI_MOUSE_HWHEEL) m_scrollAccum.x -= wheel;
		}
	}

	std::optional<i64> Mouse::OnMessage(const WindowMessage& msg)
	{
		switch (msg.Msg)
		{
		case WM_MOUSEMOVE:
			m_position = Vector2(static_cast<f32>(static_cast<i16>(LOWORD(msg.LParam))), static_cast<f32>(static_cast<i16>(HIWORD(msg.LParam))));
			break;

		case WM_SETCURSOR:
			if (m_cursorHidden && LOWORD(msg.LParam) == HTCLIENT)
			{
				::SetCursor(nullptr);
				return TRUE;
			}
			break;

		case WM_MOVE:
		case WM_SIZE:
			if (m_cursorLocked) ApplyCursorClip();
			break;

		default:
			break;
		}
		return std::nullopt;
	}

	void Mouse::OnAppActivated(bool active)
	{
		if (active)
		{
			if (m_cursorLocked) ApplyCursorClip();
			return;
		}

		m_down.fill(false);
		m_hasAbsolute = false;
		::ClipCursor(nullptr);
	}

	void Mouse::ApplyCursorClip() const
	{
		RECT client{};
		if (!m_hwnd || !::GetClientRect(m_hwnd, &client)) return;

		POINT centre{ (client.left + client.right) / 2, (client.top + client.bottom) / 2 };
		::ClientToScreen(m_hwnd, &centre);
		const RECT clip{ centre.x, centre.y, centre.x + 1, centre.y + 1 };
		::ClipCursor(&clip);
	}

	bool Mouse::IsCursorOverClient() const noexcept
	{
		if (m_cursorLocked) return true;

		POINT cursor{};
		if (!m_hwnd || !::GetCursorPos(&cursor) || ::WindowFromPoint(cursor) != m_hwnd) return false;

		RECT client{};
		::GetClientRect(m_hwnd, &client);
		::ScreenToClient(m_hwnd, &cursor);
		return ::PtInRect(&client, cursor);
	}
}
