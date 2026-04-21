#pragma once
#include "Graphics/RenderWindow.h"
#include "Application/WindowManager.h"
#include <memory>
#include <unordered_map>

namespace koi
{
	using RenderWindowHandle = Handle<RenderWindow>;

	class Renderer : NonCopyable, NonMovable, public IDeviceNotify
	{
	public:
		Renderer();
		~Renderer();

		[[nodiscard]] inline GfxDevice& GetDevice() noexcept { return *m_device; }
		[[nodiscard]] inline const GfxDevice& GetDevice() const noexcept { return *m_device; }
		[[nodiscard]] inline RenderWindow* GetRenderWindow(RenderWindowHandle h) noexcept { return m_renderWindows.Get(h); }
		[[nodiscard]] inline const RenderWindow* GetRenderWindow(RenderWindowHandle h) const noexcept { return m_renderWindows.Get(h); }

		RenderWindowHandle AddWindow(WindowHandle handle, bool vsync);
		RenderWindowHandle FindWindow(WindowHandle w) const noexcept;
		void RemoveWindow(WindowHandle handle);

		void BeginFrame();
		void EndFrame();

	private:
		void OnDeviceLost() override;
		void OnDeviceRestored() override;

	private:
		using WindowMap = std::unordered_map<WindowHandle, RenderWindowHandle>;

		std::unique_ptr<GfxDevice>  m_device;
		HandlePool<RenderWindow>    m_renderWindows;
		WindowMap                   m_windowMap;
	};
}
