#pragma once
#include "Platform/Window.h"

namespace koi::gfx
{
	class IRenderWindow
	{
	public:
		IRenderWindow(Window& window) : m_window(&window) {}
		virtual ~IRenderWindow() {}

	protected:
		Window* m_window;
	};
}
