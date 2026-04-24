#pragma once
#include "Platform/Window.h"
#include "Core/Handle.h"

namespace koi
{
	using WindowHandle = Handle<Window>;

	class WindowManager : public NonCopyable
	{
	public:
		std::pair<WindowHandle, Window*> CreateWindow(const WindowDesc& desc);
		void DestroyWindow(WindowHandle handle);

		std::pair<WindowHandle, Window*> CreateMainWindow(const WindowDesc& desc);  // For internal use only

		[[nodiscard]] inline Window* GetWindow(WindowHandle handle) { return m_pool.Get(handle); }
		[[nodiscard]] inline const Window* GetWindow(WindowHandle handle) const { return m_pool.Get(handle); }

		[[nodiscard]] inline WindowHandle GetMainWndowHandle() const { return m_mainWindow; }
		[[nodiscard]] inline Window* GetMainWindow() { return m_pool.Get(m_mainWindow); }

		[[nodiscard]] bool AllClosed() const;
		void DestroyClosedWindows();

	private:
		HandlePool<Window> m_pool;
		WindowHandle       m_mainWindow;
	};
}
