#include "Platform/Application.h"

namespace koi
{
	Window& Application::CreateWindow(const WindowDesc& desc)
	{
		auto& window = m_windows.emplace_back(std::make_unique<Window>(desc));
		window->Show();
		return *window;
	}

	bool Application::Tick()
	{
		for (auto& window : m_windows)
		{
			std::ignore = window->ProcessMessages();
		}

		std::erase_if(m_windows, [](auto& window) { return !window->IsAlive(); });

		return !m_windows.empty();
	}
}