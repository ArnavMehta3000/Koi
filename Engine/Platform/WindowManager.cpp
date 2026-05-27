#include "Platform/WindowManager.h"
#include "Core/Profiling.h"
#include <cassert>

namespace koi
{
	std::pair<WindowHandle, Window*> WindowManager::CreateWindow(const WindowDesc& desc)
	{
		ZoneScoped;
		WindowHandle handle = m_pool.Emplace(desc);
		return { handle, GetWindow(handle) };
	}

	void WindowManager::DestroyWindow(WindowHandle handle)
	{
		ZoneScoped;
		if (Window* w = GetWindow(handle)) [[likely]]
		{
			w->RequestClose();
			m_pool.Erase(handle);
		}
	}

	std::pair<WindowHandle, Window*> WindowManager::CreateMainWindow(const WindowDesc& desc)
	{
		ZoneScoped;
		assert(!m_mainWindow && "Main window already created");

		auto [handle, window] = CreateWindow(desc);
		m_mainWindow = handle;

		return { m_mainWindow, window };
	}

	bool WindowManager::AllClosed() const
	{
		ZoneScoped;
		if (m_pool.GetSize() == 0) return true;

		for (const Window& w : m_pool.GetData())
		{
			if (!w.ShouldClose()) return false;  // at least one still open
		}
		return true;
	}

	void WindowManager::DestroyClosedWindows()
	{
		ZoneScoped;
		std::vector<WindowHandle> toDestroy;
		m_pool.ForEach([&](WindowHandle h, Window& w)
		{
			if (w.ShouldClose()) toDestroy.push_back(h);
		});

		for (auto h : toDestroy) DestroyWindow(h);
	}
}
