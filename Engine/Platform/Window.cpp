#include "Platform/Window.h"
#include "Core/Logger.h"
#include "Memory/MemoryTracker.h"
#include <windowsx.h>
#include <dwmapi.h>

#undef IsMinimized
#undef IsMaximized

#ifndef GLFW_INCLUDE_NONE
	#define GLFW_INCLUDE_NONE
#endif

#include <GLFW/glfw3.h>
#include <cassert>
#include <stdexcept>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include "Platform/Win32.h"
#include <windowsx.h>
#include <dwmapi.h>
#undef IsMinimized
#undef IsMaximized

namespace koi
{
	namespace
	{
		static GLFWmonitor* SelectMonitor(i32 index)
		{
			i32 count = 0;
			if (GLFWmonitor** monitors = ::glfwGetMonitors(&count))
			{
				assert(count != 0);  // We should have at least one monitor

				if (index < 0 || index >= count)
					index = 0;

				return monitors[index];
			}

			return nullptr;
		}
	}

	Window::Window(const WindowDesc& desc)
		: m_mode(desc.Mode)
		, m_width(desc.Width)
		, m_height(desc.Height)
		, m_title(desc.Title)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Platform);
		if (s_windowCount == 0)
		{
			::glfwSetErrorCallback([](int code, const char* desc)
				{ KOI_LOG_ERROR("GLFW Error {}: {}", code, desc); });

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
	{
		other.m_window = nullptr;
		// Re-point GLFW user pointer to the new address
		if (m_window) ::glfwSetWindowUserPointer(m_window, this);
	}

	Window& Window::operator=(Window&& other) noexcept
	{
		KOI_MEM_TAG_SCOPE(MemTag::Platform);
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
		if (m_window)
			::glfwSetWindowShouldClose(m_window, GLFW_TRUE);
	}

	void Window::SetMode(WindowMode newMode)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Platform);

		if (newMode == m_mode)
			return;

		// Save window bounds before switching
		if (m_mode == WindowMode::Windowed || m_mode == WindowMode::Borderless)
		{
			::glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
			i32 w, h;
			::glfwGetWindowSize(m_window, &w, &h);
			m_savedW = static_cast<u32>(w);
			m_savedH = static_cast<u32>(h);
		}

		GLFWmonitor* monitor = SelectMonitor(0);
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
	}

	void Window::SetTitle(std::string_view title)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Platform);
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

	void Window::PollEvents() noexcept
	{
		::glfwPollEvents();
	}

	void Window::Create(const WindowDesc& desc)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Platform);
		::glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		::glfwWindowHint(GLFW_RESIZABLE, desc.Resizable ? GLFW_TRUE : GLFW_FALSE);

		GLFWmonitor* monitor = nullptr;
		i32 w = static_cast<i32>(desc.Width);
		i32 h = static_cast<i32>(desc.Height);

		switch (desc.Mode)
		{
		case WindowMode::Windowed:
			::glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
			break;

		case WindowMode::Borderless:
		{
			::glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
			monitor = SelectMonitor(desc.MonitorID);
			const GLFWvidmode* vm = ::glfwGetVideoMode(monitor);
			i32 mx, my;
			::glfwGetMonitorPos(monitor, &mx, &my);
			::glfwWindowHint(GLFW_POSITION_X, mx + (vm->width - w) / 2);
			::glfwWindowHint(GLFW_POSITION_Y, my + (vm->height - h) / 2);
			monitor = nullptr;
			break;
		}

		case WindowMode::WindowedFullscreen:
		{
			::glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

			monitor = SelectMonitor(desc.MonitorID);
			const GLFWvidmode* vm = ::glfwGetVideoMode(monitor);

			w = vm->width;
			h = vm->height;

			i32 mx, my;
			::glfwGetMonitorPos(monitor, &mx, &my);
			::glfwWindowHint(GLFW_POSITION_X, mx);
			::glfwWindowHint(GLFW_POSITION_Y, my);
			break;
		}

		case WindowMode::ExclusiveFullscreen:
		{
			::glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);

			monitor = SelectMonitor(desc.MonitorID);
			const GLFWvidmode* vm = ::glfwGetVideoMode(monitor);

			::glfwWindowHint(GLFW_RED_BITS    , vm->redBits);
			::glfwWindowHint(GLFW_GREEN_BITS  , vm->greenBits);
			::glfwWindowHint(GLFW_BLUE_BITS   , vm->blueBits);
			::glfwWindowHint(GLFW_REFRESH_RATE, vm->refreshRate);

			// [TODO]: Handle fullscreen state change in DXGI/D3D swapchain
			break;
		}
		}

		m_window = ::glfwCreateWindow(w, h, m_title.c_str(), monitor, nullptr);
		assert(m_window && "Failed to create GLFW window");

		m_width = static_cast<u32>(w);
		m_height = static_cast<u32>(h);

		::glfwSetWindowUserPointer(m_window, this);
		::glfwSetFramebufferSizeCallback(m_window, &Window::OnResize);
		::glfwSetWindowCloseCallback(m_window, &Window::OnClose);

		::glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
		m_savedW= m_width;
		m_savedH = m_height;

		HWND hwnd = ::glfwGetWin32Window(m_window);
		assert(hwnd && "Failed to get Win32 window handle");
		m_nativeHandle = hwnd;

		BOOL enable = TRUE;
		::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &enable, sizeof(enable));
		::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
			SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

		KOI_LOG_TRACE("Window created: {}({}x{})", m_title, m_width, m_height);
	}

	void Window::Destroy()
	{
		KOI_MEM_TAG_SCOPE(MemTag::Platform);
		if (m_window)
		{
			::glfwDestroyWindow(m_window);
			m_window = nullptr;
			m_nativeHandle = nullptr;

			KOI_LOG_TRACE("Window destroyed: {}", m_title);
		}
	}

	void Window::OnResize(GLFWwindow* w, i32 width, i32 height) noexcept
	{
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
		if (Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w)))
		{
			if (self->m_onClose)
				self->m_onClose();
		}
	}

	void Window::OnFocus(GLFWwindow* w, i32 focused) noexcept
	{
		if (Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w)))
		{
			if (self->m_onFocus)
				self->m_onFocus(focused == GLFW_TRUE);
		}
	}
}
