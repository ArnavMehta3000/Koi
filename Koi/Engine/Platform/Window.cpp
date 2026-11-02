#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Win32.h"
#include "Koi/Engine/Text/StringUtils.h"

#include <dwmapi.h>
#include <shellapi.h>

#include <algorithm>
#include <stdexcept>

namespace koi
{
	namespace
	{
		constexpr const wchar_t* kClassName = L"KoiWindow";
		constexpr DWORD kExStyle = WS_EX_APPWINDOW;

		DWORD StyleFor(WindowMode mode, bool resizable)
		{
			const DWORD sizing = resizable ? (WS_THICKFRAME | WS_MAXIMIZEBOX) : 0;
			switch (mode)
			{
			case WindowMode::Windowed:   return (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)) | sizing;
			case WindowMode::Borderless: return WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | sizing;  // caption keeps snap + animations, WM_NCCALCSIZE hides it
			default:                     return WS_POPUP;
			}
		}

		RECT PrimaryWorkArea()
		{
			MONITORINFO info{ .cbSize = sizeof(MONITORINFO) };
			::GetMonitorInfoW(::MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &info);
			return info.rcWork;
		}
	}

	Window::Window(const WindowDesc& desc)
		: m_mode(desc.Mode)
		, m_resizable(desc.Resizable)
		, m_title(desc.Title)
	{
		ZoneScoped;
		if (s_windowCount == 0)
		{
			::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

			const WNDCLASSEXW wc
			{
				.cbSize        = sizeof(WNDCLASSEXW),
				.style         = CS_HREDRAW | CS_VREDRAW,
				.lpfnWndProc   = &Window::WndProc,
				.hInstance     = ::GetModuleHandleW(nullptr),
				.hIcon         = ::LoadIconW(nullptr, IDI_APPLICATION),
				.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW),
				.lpszClassName = kClassName,
			};
			if (!::RegisterClassExW(&wc))
			{
				throw std::runtime_error("Failed to register the window class");
			}
		}

		Create(desc);
		s_windowCount++;
	}

	Window::~Window()
	{
		ZoneScoped;
		if (m_hwnd)
		{
			Destroy();
			if (--s_windowCount == 0) ::UnregisterClassW(kClassName, ::GetModuleHandleW(nullptr));
		}
	}

	void Window::SetMode(WindowMode newMode)
	{
		ZoneScoped;
		if (newMode == m_mode) return;

		if (m_mode == WindowMode::Windowed || m_mode == WindowMode::Borderless)
		{
			RECT rect{};
			::GetWindowRect(m_hwnd, &rect);
			m_savedX = rect.left;
			m_savedY = rect.top;
			m_savedW = rect.right - rect.left;
			m_savedH = rect.bottom - rect.top;
		}

		m_mode = newMode;
		ApplyMode(newMode);
		KOI_LOG(Platform, Debug, "Window mode changed to {}", static_cast<i32>(newMode));
	}

	void Window::ApplyMode(WindowMode mode)
	{
		const DWORD visible = ::IsWindowVisible(m_hwnd) ? WS_VISIBLE : 0;
		::SetWindowLongPtrW(m_hwnd, GWL_STYLE, static_cast<LONG_PTR>(StyleFor(mode, m_resizable) | visible));

		// A one pixel frame brings the DWM shadow back to the borderless window
		const MARGINS margins = mode == WindowMode::Borderless ? MARGINS{ 1, 1, 1, 1 } : MARGINS{};
		::DwmExtendFrameIntoClientArea(m_hwnd, &margins);

		constexpr UINT flags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER;
		if (mode == WindowMode::Windowed || mode == WindowMode::Borderless)
		{
			::SetWindowPos(m_hwnd, nullptr, m_savedX, m_savedY, m_savedW, m_savedH, flags);
			return;
		}

		// [TODO]: ExclusiveFullscreen should go through IDXGISwapChain::SetFullscreenState(TRUE)
		MONITORINFO info{ .cbSize = sizeof(MONITORINFO) };
		::GetMonitorInfoW(::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &info);
		const RECT& r = info.rcMonitor;
		::SetWindowPos(m_hwnd, nullptr, r.left, r.top, r.right - r.left, r.bottom - r.top, flags);
	}

	void Window::SetTitle(std::string_view title)
	{
		ZoneScoped;
		m_title.assign(title);
		if (m_hwnd) ::SetWindowTextW(m_hwnd, utils::ToWide(m_title).c_str());
	}

	void Window::Minimize() { if (m_hwnd) ::ShowWindow(m_hwnd, SW_MINIMIZE); }
	void Window::Maximize() { if (m_hwnd) ::ShowWindow(m_hwnd, SW_MAXIMIZE); }
	void Window::Restore()  { if (m_hwnd) ::ShowWindow(m_hwnd, SW_RESTORE);  }

	void Window::Focus()
	{
		if (!m_hwnd) return;
		::BringWindowToTop(m_hwnd);
		::SetForegroundWindow(m_hwnd);
		::SetFocus(m_hwnd);
	}

	bool Window::IsFocused() const noexcept   { return m_hwnd && ::GetActiveWindow() == m_hwnd; }
	bool Window::IsVisible() const noexcept   { return m_hwnd && ::IsWindowVisible(m_hwnd); }
	bool Window::IsMinimized() const noexcept { return m_hwnd && ::IsIconic(m_hwnd); }
	bool Window::IsMaximized() const noexcept { return m_hwnd && ::IsZoomed(m_hwnd); }

	bool Window::IsHovered() const noexcept
	{
		POINT cursor{};
		return m_hwnd && ::GetCursorPos(&cursor) && ::WindowFromPoint(cursor) == m_hwnd;
	}

	void Window::SetSize(u32 width, u32 height)
	{
		if (!m_hwnd) return;

		RECT rect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		if (m_mode == WindowMode::Windowed)
		{
			::AdjustWindowRectExForDpi(&rect, StyleFor(m_mode, m_resizable), FALSE, kExStyle, ::GetDpiForWindow(m_hwnd));
		}
		::SetWindowPos(m_hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}

	f32 Window::GetContentScale() const noexcept
	{
		return m_hwnd ? static_cast<f32>(::GetDpiForWindow(m_hwnd)) / USER_DEFAULT_SCREEN_DPI : 1.0f;
	}

	u32 Window::AddMessageHook(MessageHook hook)
	{
		const u32 id = m_nextHookId++;
		m_hooks.push_back({ id, std::move(hook) });
		return id;
	}

	void Window::RemoveMessageHook(u32 id)
	{
		std::erase_if(m_hooks, [id](const Hook& hook) { return hook.Id == id; });
	}

	void Window::PollEvents() noexcept
	{
		ZoneScoped;
		MSG msg{};
		while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}
	}

	void Window::Create(const WindowDesc& desc)
	{
		ZoneScoped;

		// Created hidden on the primary monitor, so its DPI is known before sizing
		const RECT work  = PrimaryWorkArea();
		const DWORD style = StyleFor(desc.Mode, desc.Resizable);
		const std::wstring title = utils::ToWide(m_title);

		::CreateWindowExW(kExStyle, kClassName, title.c_str(), style,
			work.left, work.top, 640, 480,
			nullptr, nullptr, ::GetModuleHandleW(nullptr), this);
		if (!m_hwnd)
		{
			throw std::runtime_error("Failed to create the window");
		}

		// Logical size: most of the work area, using desc size as a floor
		const u32 dpi   = ::GetDpiForWindow(m_hwnd);
		const f32 scale = static_cast<f32>(dpi) / USER_DEFAULT_SCREEN_DPI;
		const i32 workW = work.right - work.left;
		const i32 workH = work.bottom - work.top;
		const auto logical = [&](i32 extent, f32 fraction) { return static_cast<i32>(static_cast<f32>(extent) * fraction / scale); };
		const i32 w = std::min(std::max(static_cast<i32>(desc.Width),  logical(workW, 0.85f)), logical(workW, 0.9f));
		const i32 h = std::min(std::max(static_cast<i32>(desc.Height), logical(workH, 0.85f)), logical(workH, 0.9f));

		RECT rect{ 0, 0, static_cast<LONG>(static_cast<f32>(w) * scale), static_cast<LONG>(static_cast<f32>(h) * scale) };
		::AdjustWindowRectExForDpi(&rect, StyleFor(WindowMode::Windowed, desc.Resizable), FALSE, kExStyle, dpi);
		m_savedW = rect.right - rect.left;
		m_savedH = rect.bottom - rect.top;
		m_savedX = work.left + std::max(0, (workW - m_savedW) / 2);
		m_savedY = work.top  + std::max(0, (workH - m_savedH) / 2);
		ApplyMode(desc.Mode);

		BOOL dark = TRUE;
		::DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
		::DragAcceptFiles(m_hwnd, TRUE);

		RECT client{};
		::GetClientRect(m_hwnd, &client);
		m_width  = static_cast<u32>(client.right - client.left);
		m_height = static_cast<u32>(client.bottom - client.top);

		::ShowWindow(m_hwnd, SW_SHOWNORMAL);
		KOI_LOG(Platform, Info, "Window created: '{}' ({}x{}, {:.0f}% scale)", m_title, m_width, m_height, GetContentScale() * 100.0f);
	}

	void Window::Destroy()
	{
		ZoneScoped;
		if (!m_hwnd) return;

		::DestroyWindow(m_hwnd);
		m_hwnd = nullptr;
		KOI_LOG(Platform, Debug, "Window destroyed: '{}'", m_title);
	}
}
