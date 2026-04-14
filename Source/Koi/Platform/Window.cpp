#include "Platform/Window.h"
#include <windowsx.h>
#include <dwmapi.h>

namespace koi
{
	Window::Window(const WindowDesc& desc)
		: m_hWnd(nullptr)
		, m_width(desc.Width)
		, m_height(desc.Height)
	{
		s_hInstance = GetModuleHandleW(nullptr);

		if (s_windowCount == 0)
		{
			WNDCLASSEXW windowClass
			{
				.cbSize        = sizeof(windowClass),
				.style         = CS_HREDRAW | CS_VREDRAW,
				.lpfnWndProc   = WndProcStatic,
				.hInstance     = s_hInstance,
				.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW),
				.lpszClassName = s_className
			};
			
			[[maybe_unused]] auto result = ::RegisterClassExW(&windowClass);
			KOI_ASSERT_MSG(result, "Failed to register window class");
		}
		++s_windowCount;

		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!desc.Resizable)
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);

		RECT rc{ 0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height) };
		::AdjustWindowRect(&rc, style, FALSE);

		// Centre the window
		i32 x = CW_USEDEFAULT, y = CW_USEDEFAULT;
		i32 sw =::GetSystemMetrics(SM_CXSCREEN);
		i32 sh =::GetSystemMetrics(SM_CYSCREEN);
		x = (sw - (rc.right - rc.left)) / 2;
		y = (sh - (rc.bottom - rc.top)) / 2;

		m_hWnd = ::CreateWindowExW(
			0, s_className,
			desc.Title.c_str(),
			style, x, y,
			rc.right - rc.left,
			rc.bottom - rc.top,
			nullptr, nullptr,
			s_hInstance,
			this
		);
		KOI_ASSERT_MSG(m_hWnd, "Failed to create window");
	}

	Window::~Window()
	{
		if (m_hWnd)
		{
			::DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
		}

		if (--s_windowCount == 0)
		{
			::UnregisterClassW(s_className, s_hInstance);
		}
	}

	bool Window::ProcessMessages() const
	{
		MSG msg{};
		while (::PeekMessageW(&msg, m_hWnd, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}
		return m_alive;
	}
	
	void Window::SetTitle(const std::wstring& title) const
	{
		::SetWindowTextW(m_hWnd, title.c_str());
	}

	void Window::Show(i32 showCmd) const
	{
		// Set window title bars to be dark mode
		BOOL enable = TRUE;
		::DwmSetWindowAttribute(m_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(enable));
		::SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

		::ShowWindow(m_hWnd, showCmd);
		::UpdateWindow(m_hWnd);
	}

	LRESULT Window::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
	{
		Window* self = nullptr;
		if (msg == WM_NCCREATE)
		{
			auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
			self = static_cast<Window*>(cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
			self->m_hWnd = hwnd;
		}
		else {
			self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		}

		return self ? self->WndProc(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
	}
	
	LRESULT Window::WndProc(UINT msg, WPARAM wp, LPARAM lp)
	{
		switch (msg)
		{
		case WM_CLOSE:
		{
			m_alive = false;
			if (OnCloseCallback) OnCloseCallback();
			return 0;
		}
		
		case WM_DESTROY:
		{
			m_hWnd = nullptr;
			return 0;
		}

		case WM_SETFOCUS:
		{
			m_focused = true;
			return 0;
		}
		
		case WM_KILLFOCUS:
		{
			m_focused = false;
			return 0;
		}

		case WM_ENTERSIZEMOVE:
		{
			m_resizing = true;
			return 0;
		}
		case WM_EXITSIZEMOVE:
		{
			m_resizing = false;
			[[fallthrough]];
		}
		case WM_SIZE:
		{
			if (wp == SIZE_MINIMIZED) return 0;

			u32 w = LOWORD(lp), h = HIWORD(lp);
			if (w > 0  && h > 0 && (w != m_width || h != m_height))
			{
				m_width = w; m_height = h;

				if (!m_resizing && OnResizeCallback)
					OnResizeCallback({ w, h });
			}
			return 0;
		}

		default: return DefWindowProcW(m_hWnd, msg, wp, lp);
		}
	}
}
