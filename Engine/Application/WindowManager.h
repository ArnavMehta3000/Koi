#pragma once
#include "Platform/Window.h"
#include "Core/Handle.h"

namespace koi
{
	using WindowHandle = Handle<Window>;

	class WindowManager : public NonCopyable
	{
	public:
		WindowHandle Create(const WindowDesc& desc);
		void         Destroy(WindowHandle handle);

		WindowHandle CreateMainWindow(const WindowDesc& desc);  // For internal use only

		[[nodiscard]] inline Window* Get(WindowHandle handle) { return m_pool.Get(handle); }
		[[nodiscard]] inline const Window* Get(WindowHandle handle) const { return m_pool.Get(handle); }

		[[nodiscard]] inline WindowHandle GetMainHandle() const { return m_mainWindow; }
		[[nodiscard]] inline Window* GetMain() { return m_pool.Get(m_mainWindow); }

		[[nodiscard]] bool AllClosed() const;
		void DestroyClosedWindows();

	private:
		HandlePool<Window> m_pool;
		WindowHandle       m_mainWindow;
	};
}
