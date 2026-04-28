#pragma once
#include "Platform/Window.h"
#include "Core/Handle.h"

namespace koi
{
	using WindowHandle = Handle<Window>;

	class WindowManager : public NonCopyable
	{
		friend class Engine;
	public:
		std::pair<WindowHandle, Window*> CreateWindow(const WindowDesc& desc);   // NOTE: The pointer is not safe to store, but okay for temporary actions (same with create main window)
		void DestroyWindow(WindowHandle handle);


		[[nodiscard]] inline Window* GetWindow(WindowHandle handle) { return m_pool.Get(handle); }
		[[nodiscard]] inline const Window* GetWindow(WindowHandle handle) const { return m_pool.Get(handle); }

		[[nodiscard]] inline WindowHandle GetMainWndowHandle() const { return m_mainWindow; }
		[[nodiscard]] inline Window* GetMainWindow() { return m_pool.Get(m_mainWindow); }

		[[nodiscard]] bool AllClosed() const;
		void DestroyClosedWindows();

	private:
		std::pair<WindowHandle, Window*> CreateMainWindow(const WindowDesc& desc);

	private:
		HandlePool<Window> m_pool;
		WindowHandle       m_mainWindow;
	};
}
