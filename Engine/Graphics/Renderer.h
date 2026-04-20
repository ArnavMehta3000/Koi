#pragma once
#include "Graphics/RenderWindow.h"
#include "Application/WindowManager.h"
#include <memory>
#include <unordered_map>

namespace koi
{
	using RenderWindowHandle = Handle<RenderWindow>;

	class Renderer : NonCopyable, NonMovable
	{
	public:
		Renderer();
		~Renderer();

		[[nodiscard]] inline RenderWindow* Get(RenderWindowHandle h) { return m_renderWindows.Get(h); }
		[[nodiscard]] inline const RenderWindow* Get(RenderWindowHandle h) const { return m_renderWindows.Get(h); }

		RenderWindowHandle AddWindow(WindowHandle handle);
		void RemoveWindow(WindowHandle handle);

		void BeginFrame();
		void EndFrame();

	private:
		using WindowMap = std::unordered_map<WindowHandle, RenderWindowHandle>;

		std::unique_ptr<GfxDevice>  m_device;
		HandlePool<RenderWindow>    m_renderWindows;
		WindowMap                   m_windowMap;
	};
}
