#include "Platform/Window.h"
#include "Core/Logging/Logger.h"
#include <windowsx.h>
#include <dwmapi.h>


namespace koi
{
	Window::Window(const WindowDesc& desc)
		: m_mode(desc.Mode)
		, m_width(desc.Width)
		, m_height(desc.Height)
		, m_title(desc.Title)
	{
		if (s_windowCount == 0)
		{
			::glfwSetErrorCallback([](int code, const char* desc)
				{ KOI_LOG_ERROR("GLFW Error {}: {}", code, desc); });

			if (!::glfwInit())
			{
				KOI_VERIFY(false, "Failed to initialize GLFW");
			}
		}

		Create(desc);
		s_windowCount++;
	}

	Window::~Window()
	{
		Destroy();
		if (--s_windowCount == 0)
		{
			::glfwTerminate();
		}
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
		m_title.assign(title);
		if (m_window)
			::glfwSetWindowTitle(m_window, m_title.c_str());
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
			::glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
			break;

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
		KOI_VERIFY_MSG(m_window, "Failed to create GLFW window");

		m_width = static_cast<u32>(w);
		m_height = static_cast<u32>(h);

		::glfwSetWindowUserPointer(m_window, this);
		::glfwSetFramebufferSizeCallback(m_window, &Window::OnResize);
		::glfwSetWindowCloseCallback(m_window, &Window::OnClose);

		m_hWnd = ::glfwGetWin32Window(m_window);
		KOI_ASSERT_MSG(m_hWnd, "Failed to get GLFW window Win32 handle");

		::glfwGetWindowPos(m_window, &m_savedX, &m_savedY);
		m_savedW= m_width;
		m_savedH = m_height;
	}

	void Window::Destroy()
	{
		if (m_window)
		{
			::glfwDestroyWindow(m_window);
			m_window = nullptr;
			m_hWnd = nullptr;
		}
	}

	GLFWmonitor* Window::SelectMonitor(i32 index) const
	{
		i32 count = 0;
		if (GLFWmonitor** monitors = ::glfwGetMonitors(&count))
		{
			KOI_ASSERT(count == 0);  // We should have at least one monitor
		
			if (index < 0 || index >= count)
				index = 0;

			return monitors[index];
		}

		return nullptr;
	}

	void Window::OnResize(GLFWwindow* w, i32 width, i32 height) noexcept
	{
		if (Window* self = static_cast<Window*>(::glfwGetWindowUserPointer(w)))
		{
			self->m_width = static_cast<u32>(width);
			self->m_height = static_cast<u32>(height);

			if (self->m_onResize)
				self->m_onResize(self->m_width, self->m_width);
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
}
