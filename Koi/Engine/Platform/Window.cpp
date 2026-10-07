#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Win32.h"

#ifndef GLFW_INCLUDE_NONE
	#define GLFW_INCLUDE_NONE
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dwmapi.h>

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace koi
{
	namespace
	{
		struct WorkArea
		{
			i32 X      = 0;
			i32 Y      = 0;
			i32 Width  = 1280;
			i32 Height = 720;
			f32 Scale  = 1.0f;  // monitor content scale ->  the DPI setting
		};

		WorkArea PrimaryWorkArea()
		{
			WorkArea area;
			if (GLFWmonitor* monitor = ::glfwGetPrimaryMonitor())
			{
				::glfwGetMonitorWorkarea(monitor, &area.X, &area.Y, &area.Width, &area.Height);
				f32 scaleY = 1.0f;
				::glfwGetMonitorContentScale(monitor, &area.Scale, &scaleY);
			}
			return area;
		}

		// Centers the window, frame included, on the primary monitor's work area
		void CenterOnWorkArea(GLFWwindow* window, const WorkArea& area)
		{
			i32 width = 0, height = 0;
			i32 left = 0, top = 0, right = 0, bottom = 0;
			::glfwGetWindowSize(window, &width, &height);
			::glfwGetWindowFrameSize(window, &left, &top, &right, &bottom);

			const i32 outerWidth  = width + left + right;
			const i32 outerHeight = height + top + bottom;
			::glfwSetWindowPos(window,
				area.X + std::max(0, (area.Width - outerWidth) / 2) + left,
				area.Y + std::max(0, (area.Height - outerHeight) / 2) + top);
		}
	}

	Window::Window(const WindowDesc& desc)
		: m_mode(desc.Mode)
		, m_width(desc.Width)
		, m_height(desc.Height)
		, m_title(desc.Title)
	{
		ZoneScoped;
		if (s_windowCount == 0)
		{
			::glfwSetErrorCallback([](int code, const char* desc)
			{
				// 65537 (not initialized) fires from the ImGui GLFW backend during shutdown and is harmless
				if (code == 65537) { KOI_LOG_TRACE("GLFW Error {}: {}", code, desc); return; }
				KOI_LOG_ERROR("GLFW Error {}: {}", code, desc);
			});

			::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

			if (!::glfwInit())
			{
				throw std::runtime_error("Failed to initialize GLFW");
			}
		}

		Create(desc);
		s_windowCount++;
	}

	Window::~Window()
	{
		ZoneScoped;
		if (m_window)
		{
			Destroy();
			if (--s_windowCount == 0) ::glfwTerminate();
		}
	}

	Window::Window(Window&& other) noexcept
		: m_window(other.m_window)
		, m_width(other.m_width)
		, m_height(other.m_height)
		, m_mode(other.m_mode)
		, m_title(std::move(other.m_title))
		, m_savedX(other.m_savedX), m_savedY(other.m_savedY)
		, m_savedW(other.m_savedW), m_savedH(other.m_savedH)
		, m_onResize(std::move(other.m_onResize))
		, m_onClose(std::move(other.m_onClose))
		, m_onFocus(std::move(other.m_onFocus))
		, m_onDrop(std::move(other.m_onDrop))
	{
		other.m_window = nullptr;
		// Re-point GLFW user pointer to the new address
		if (m_window) ::glfwSetWindowUserPointer(m_window, this);
	}

	Window& Window::operator=(Window&& other) noexcept
	{
		if (this != &other)
		{
			Destroy();

			m_window   = other.m_window;
			m_width    = other.m_width;
			m_height   = other.m_height;
			m_mode     = other.m_mode;
			m_title    = std::move(other.m_title);
			m_savedX   = other.m_savedX; m_savedY = other.m_savedY;
			m_savedW   = other.m_savedW; m_savedH = other.m_savedH;
			m_onResize = std::move(other.m_onResize);
			m_onClose  = std::move(other.m_onClose);
			m_onFocus  = std::move(other.m_onFocus);
			m_onDrop   = std::move(other.m_onDrop);

			other.m_window = nullptr;
			if (m_window) ::glfwSetWindowUserPointer(m_window, this);
		}
		return *this;
	}

	bool Window::ShouldClose() const noexcept
	{
		return m_window? ::glfwWindowShouldClose(m_window) : true;
	}

	void Window::RequestClose()
	{
		ZoneScoped;
		if (m_window)
			::glfwSetWindowShouldClose(m_window, GLFW_TRUE);
	}

	void Window::SetMode(WindowMode newMode)
	{
		ZoneScoped;

		if (newMode == m_mode)
			return;

		// Save window bounds before switching
		if (m_mode == WindowMode::Windowed || m_mode == WindowMode::Borderless)
		{
			::glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
			::glfwGetWindowSize(m_window, &m_savedW, &m_savedH);
		}

		GLFWmonitor* monitor = ::glfwGetPrimaryMonitor();
		const GLFWvidmode* vm = ::glfwGetVideoMode(monitor);

		switch (newMode)
		{
		case WindowMode::Windowed:
		{
			::glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
			::glfwSetWindowMonitor(m_window, nullptr,
				m_savedX, m_savedY, m_savedW, m_savedH,
				GLFW_DONT_CARE);
			break;
		}

		case WindowMode::Borderless:
		{
			::glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
			::glfwSetWindowMonitor(m_window, nullptr,
				m_savedX, m_savedY, m_savedW, m_savedH,
				GLFW_DONT_CARE);
			break;
		}

		case WindowMode::WindowedFullscreen:
		{
			::glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
			i32 mx, my;
			::glfwGetMonitorPos(monitor, &mx, &my);
			::glfwSetWindowMonitor(m_window, nullptr, mx, my,
				vm->width, vm->height, GLFW_DONT_CARE);
			break;
		}

		case WindowMode::ExclusiveFullscreen:
		{
			::glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
			::glfwSetWindowMonitor(m_window, monitor, 0, 0,
				vm->width, vm->height, vm->refreshRate);

			// [TODO]: Call IDXGISwapChain::SetFullscreenState(TRUE)
			break;
		}
		}

		m_mode = newMode;
		KOI_LOG_DEBUG("Window mode changed to {}", static_cast<i32>(newMode));
	}

	void Window::SetTitle(std::string_view title)
	{
		ZoneScoped;
		m_title.assign(title);
		if (m_window)
			::glfwSetWindowTitle(m_window, m_title.c_str());
	}

	void Window::Minimize()
	{
		if (m_window) ::glfwIconifyWindow(m_window);
	}

	void Window::Maximize()
	{
		if (m_window) ::glfwMaximizeWindow(m_window);
	}

	void Window::Restore()
	{
		if (m_window) ::glfwRestoreWindow(m_window);
	}

	bool Window::IsMinimized() const noexcept
	{
		return m_window && ::glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE;
	}

	bool Window::IsMaximized() const noexcept
	{
		return m_window && ::glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED) == GLFW_TRUE;
	}

	void Window::Focus()
	{
		if (m_window) ::glfwFocusWindow(m_window);
	}

	bool Window::IsFocused() const noexcept
	{
		return m_window && ::glfwGetWindowAttrib(m_window, GLFW_FOCUSED) == GLFW_TRUE;
	}

	bool Window::IsHovered() const noexcept
	{
		return m_window && ::glfwGetWindowAttrib(m_window, GLFW_HOVERED) == GLFW_TRUE;
	}

	bool Window::IsVisible() const noexcept
	{
		return m_window && ::glfwGetWindowAttrib(m_window, GLFW_VISIBLE) == GLFW_TRUE;
	}

	void Window::SetSize(u32 width, u32 height)
	{
		if (m_window)
			::glfwSetWindowSize(m_window, width, height);
	}

	f32 Window::GetContentScale() const noexcept
	{
		f32 x = 1.0f, y = 1.0f;
		if (m_window) ::glfwGetWindowContentScale(m_window, &x, &y);
		return x;
	}

	void Window::PollEvents() noexcept
	{
		ZoneScoped;
		::glfwPollEvents();
	}

	void Window::Create(const WindowDesc& desc)
	{
		ZoneScoped;
		::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		::glfwWindowHint(GLFW_RESIZABLE, desc.Resizable ? GLFW_TRUE : GLFW_FALSE);
		::glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);  // desc size is in logical pixels, and follows DPI across monitors
		::glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);          // shown once centered

		const WorkArea area = PrimaryWorkArea();
		GLFWmonitor* fullscreenMonitor = nullptr;

		// Logical size: most of the work area, usign desc size as a floor
		const auto logical = [&](i32 extent, f32 fraction) { return static_cast<i32>(static_cast<f32>(extent) * fraction / area.Scale); };
		i32 w = std::min(std::max(static_cast<i32>(desc.Width),  logical(area.Width,  0.85f)), logical(area.Width,  0.9f));
		i32 h = std::min(std::max(static_cast<i32>(desc.Height), logical(area.Height, 0.85f)), logical(area.Height, 0.9f));

		switch (desc.Mode)
		{
		case WindowMode::Windowed:
			::glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
			break;

		case WindowMode::Borderless:
			::glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
			break;

		case WindowMode::WindowedFullscreen:
		case WindowMode::ExclusiveFullscreen:
		{
			GLFWmonitor* monitor = ::glfwGetPrimaryMonitor();
			const GLFWvidmode* vm = ::glfwGetVideoMode(monitor);
			w = vm->width;
			h = vm->height;

			if (desc.Mode == WindowMode::WindowedFullscreen)
			{
				::glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
				::glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);  // already in pixels
				break;
			}

			::glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
			::glfwWindowHint(GLFW_RED_BITS    , vm->redBits);
			::glfwWindowHint(GLFW_GREEN_BITS  , vm->greenBits);
			::glfwWindowHint(GLFW_BLUE_BITS   , vm->blueBits);
			::glfwWindowHint(GLFW_REFRESH_RATE, vm->refreshRate);
			fullscreenMonitor = monitor;

			// [TODO]: Handle fullscreen state change in DXGI/D3D swapchain
			break;
		}
		}

		m_window = ::glfwCreateWindow(w, h, m_title.c_str(), fullscreenMonitor, nullptr);
		assert(m_window && "Failed to create GLFW window");
		::glfwDefaultWindowHints();

		if (desc.Mode == WindowMode::WindowedFullscreen)
		{
			i32 mx = 0, my = 0;
			::glfwGetMonitorPos(::glfwGetPrimaryMonitor(), &mx, &my);
			::glfwSetWindowPos(m_window, mx, my);
		}
		else if (!fullscreenMonitor)
		{
			CenterOnWorkArea(m_window, area);
		}

		// The swapchain is sized in pixels, which is the scaled size
		i32 fbWidth = 0, fbHeight = 0;
		::glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
		m_width  = static_cast<u32>(fbWidth);
		m_height = static_cast<u32>(fbHeight);

		::glfwSetWindowUserPointer(m_window, this);
		::glfwSetFramebufferSizeCallback(m_window, &Window::OnResize);
		::glfwSetWindowCloseCallback(m_window, &Window::OnClose);
		::glfwSetDropCallback(m_window, &Window::OnDrop);

		::glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
		::glfwGetWindowSize(m_window, &m_savedW, &m_savedH);

		HWND hwnd = ::glfwGetWin32Window(m_window);
		assert(hwnd && "Failed to get Win32 window handle");
		m_nativeHandle = hwnd;

		BOOL enable = TRUE;
		::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(enable));
		::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		::glfwShowWindow(m_window);

		KOI_LOG_INFO("Window created: '{}' ({}x{}, {:.0f}% scale)", m_title, m_width, m_height, GetContentScale() * 100.0f);
	}

	void Window::Destroy()
	{
		ZoneScoped;
		if (m_window)
		{
			::glfwDestroyWindow(m_window);
			m_window = nullptr;
			m_nativeHandle = nullptr;

			KOI_LOG_DEBUG("Window destroyed: '{}'", m_title);
		}
	}

	void Window::OnResize(GLFWwindow* w, i32 width, i32 height) noexcept
	{
		ZoneScoped;
		if (Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w)))
		{
			self->m_width = static_cast<u32>(width);
			self->m_height = static_cast<u32>(height);

			if (self->m_onResize)
				self->m_onResize(self->m_width, self->m_height);
		}
	}

	void Window::OnClose(GLFWwindow* w) noexcept
	{
		ZoneScoped;
		if (Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w)))
		{
			KOI_LOG_DEBUG("Window close requested: '{}'", self->m_title);
			if (self->m_onClose)
				self->m_onClose();
		}
	}

	void Window::OnFocus(GLFWwindow* w, i32 focused) noexcept
	{
		ZoneScoped;
		if (Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w)))
		{
			if (self->m_onFocus)
				self->m_onFocus(focused == GLFW_TRUE);
		}
	}

	void Window::OnDrop(GLFWwindow* w, i32 count, const char** paths) noexcept
	{
		ZoneScoped;
		Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w));
		if (!self || !self->m_onDrop || count <= 0) return;

		// GLFW hands over UTF-8
		std::vector<std::filesystem::path> files;
		files.reserve(static_cast<size_t>(count));
		for (i32 i = 0; i < count; ++i)
			files.emplace_back(std::u8string_view(reinterpret_cast<const char8_t*>(paths[i])));

		self->m_onDrop(files);
	}
}
