#include "Application/WindowManager.h"

namespace koi
{
	WindowHandle WindowManager::Create(const WindowDesc& desc)
	{
		return m_pool.Emplace(desc);
	}

	void WindowManager::Destroy(WindowHandle handle)
	{
		if (Window* w = Get(handle)) [[likely]]
		{
			w->RequestClose();
			m_pool.Erase(handle);
		}
	}

	WindowHandle WindowManager::CreateMainWindow(const WindowDesc& desc)
	{
		KOI_ASSERT_MSG(!m_mainWindow, "Main window already created");
		m_mainWindow = Create(desc);
		return m_mainWindow;
	}

	bool WindowManager::AllClosed() const
	{
		if (m_pool.GetSize() == 0) return true;

		for (const Window& w : m_pool.GetData())
		{
			if (!w.ShouldClose()) return false;  // at least one still open
		}
		return true;
	}

	void WindowManager::DestroyClosedWindows()
	{
		std::vector<WindowHandle> toDestroy;
		m_pool.ForEach([&](WindowHandle h, Window& w)
		{
			if (w.ShouldClose()) toDestroy.push_back(h);
		});

		for (auto h : toDestroy) Destroy(h);
	}
}
