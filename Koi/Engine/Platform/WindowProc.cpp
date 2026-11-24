#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Core/Win32.h"
#include <shellapi.h>

// Borderless frame handling after https://github.com/melak47/BorderlessWindow

namespace koi
{
	namespace
	{
		constexpr i32 kMinClientWidth  = 640;
		constexpr i32 kMinClientHeight = 360;

		i32 LParamX(i64 lParam) { return static_cast<i16>(LOWORD(lParam)); }
		i32 LParamY(i64 lParam) { return static_cast<i16>(HIWORD(lParam)); }
	}

	i64 Window::WndProc(HWND__* hwnd, u32 msg, u64 wParam, i64 lParam) noexcept
	{
		if (msg == WM_NCCREATE)
		{
			auto* self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
			self->m_hwnd = hwnd;
			::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}

		auto* self = reinterpret_cast<Window*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (!self)
		{
			return ::DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		if (msg == WM_NCDESTROY)
		{
			::SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
			return ::DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		const WindowMessage message{ hwnd, msg, wParam, lParam };
		for (const Hook& hook : self->m_hooks)
		{
			if (const std::optional<i64> result = hook.Fn(message)) return *result;
		}

		return self->HandleMessage(msg, wParam, lParam);
	}

	i64 Window::HandleMessage(u32 msg, u64 wParam, i64 lParam)
	{
		switch (msg)
		{
		case WM_CLOSE:
		{
			KOI_LOG(Platform, Debug, "Window close requested: '{}'", m_title);
			m_shouldClose = true;
			if (m_onClose) m_onClose();
			return 0;  // destroyed by the destructor, not DefWindowProc
		}

		case WM_SIZE:
		{
			const u32 width  = LOWORD(lParam);
			const u32 height = HIWORD(lParam);
			if (wParam == SIZE_MINIMIZED || (width == m_width && height == m_height)) return 0;

			m_width  = width;
			m_height = height;
			if (m_onResize) m_onResize(m_width, m_height);
			return 0;
		}

		case WM_DPICHANGED:
		{
			if (m_mode == WindowMode::Windowed || m_mode == WindowMode::Borderless)
			{
				const RECT& r = *reinterpret_cast<const RECT*>(lParam);
				::SetWindowPos(m_hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOZORDER | SWP_NOACTIVATE);
			}
			return 0;
		}

		case WM_GETMINMAXINFO:
		{
			const i32 dpi = static_cast<i32>(::GetDpiForWindow(m_hwnd));
			RECT rect{ 0, 0, ::MulDiv(kMinClientWidth, dpi, USER_DEFAULT_SCREEN_DPI), ::MulDiv(kMinClientHeight, dpi, USER_DEFAULT_SCREEN_DPI) };
			if (m_mode == WindowMode::Windowed)
			{
				::AdjustWindowRectExForDpi(&rect, static_cast<DWORD>(::GetWindowLongPtrW(m_hwnd, GWL_STYLE)), FALSE, WS_EX_APPWINDOW, static_cast<UINT>(dpi));
			}

			auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
			info->ptMinTrackSize = { rect.right - rect.left, rect.bottom - rect.top };
			return 0;
		}

		case WM_DROPFILES:
		{
			auto* drop = reinterpret_cast<HDROP>(wParam);
			if (m_onDrop)
			{
				const UINT count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
				std::vector<fs::path> files;
				files.reserve(count);

				std::wstring buffer;
				for (UINT i = 0; i < count; ++i)
				{
					buffer.resize(::DragQueryFileW(drop, i, nullptr, 0));
					::DragQueryFileW(drop, i, buffer.data(), static_cast<UINT>(buffer.size() + 1));
					files.emplace_back(buffer);
				}
				m_onDrop(files);
			}
			::DragFinish(drop);
			return 0;
		}

		case WM_SYSCOMMAND:
		{
			// A lone Alt / F10 would otherwise park the keyboard in the (missing) menu bar
			if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
			break;
		}

		case WM_ERASEBKGND:
			return 1;

		case WM_NCCALCSIZE:
		{
			if (wParam != TRUE || m_mode != WindowMode::Borderless) break;

			// The whole window is client area. Maximized, the frame hangs off-screen, so clamp to the work area
			if (::IsZoomed(m_hwnd))
			{
				MONITORINFO info{ .cbSize = sizeof(MONITORINFO) };
				if (::GetMonitorInfoW(::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &info))
				{
					reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0] = info.rcWork;
				}
			}
			return 0;
		}

		case WM_NCHITTEST:
		{
			if (m_mode == WindowMode::Borderless) return HitTest(LParamX(lParam), LParamY(lParam));
			break;
		}
		}

		return ::DefWindowProcW(m_hwnd, msg, wParam, lParam);
	}

	i64 Window::HitTest(i32 x, i32 y) const noexcept
	{
		if (!m_resizable || ::IsZoomed(m_hwnd)) return HTCLIENT;

		RECT window{};
		if (!::GetWindowRect(m_hwnd, &window)) return HTNOWHERE;

		const UINT dpi    = ::GetDpiForWindow(m_hwnd);
		const i32 padding = ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
		const i32 borderX = ::GetSystemMetricsForDpi(SM_CXFRAME, dpi) + padding;
		const i32 borderY = ::GetSystemMetricsForDpi(SM_CYFRAME, dpi) + padding;

		enum : u8 { Left = 1, Right = 2, Top = 4, Bottom = 8 };
		const u8 edges = (x <  window.left + borderX   ? Left   : 0)
		               | (x >= window.right - borderX  ? Right  : 0)
		               | (y <  window.top + borderY    ? Top    : 0)
		               | (y >= window.bottom - borderY ? Bottom : 0);

		switch (edges)
		{
		case Left:           return HTLEFT;
		case Right:          return HTRIGHT;
		case Top:            return HTTOP;
		case Bottom:         return HTBOTTOM;
		case Top | Left:     return HTTOPLEFT;
		case Top | Right:    return HTTOPRIGHT;
		case Bottom | Left:  return HTBOTTOMLEFT;
		case Bottom | Right: return HTBOTTOMRIGHT;
		default:             return HTCLIENT;
		}
	}
}
