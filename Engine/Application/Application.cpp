#include "Application/Application.h"

namespace koi
{
	Window& Application::CreateWindow(const WindowDesc& desc)
	{
		auto& window = m_windows.emplace_back(std::make_unique<Window>(desc));
		return *window;
	}

	bool Application::Tick()
	{
		Window::PollEvents();

		std::erase_if(m_windows, [](auto& window) { return !window->ShouldClose(); });

		return !m_windows.empty();
	}
}
