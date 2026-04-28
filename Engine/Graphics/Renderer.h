#pragma once
#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/CommandAllocator.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/CommandQueue.h"
#include "Application/WindowManager.h"
#include <memory>
#include <unordered_map>

namespace koi
{
	using RenderWindowHandle = Handle<RenderWindow>;

	class Renderer : NonCopyable, NonMovable, public IDeviceNotify
	{
		using WindowMap = std::unordered_map<WindowHandle, RenderWindowHandle>;
		struct FrameContext  // Per-window rendering context
		{
			CommandAllocator Allocator;
			CommandList List;
		};
		using FrameMap = std::unordered_map<u64 /*handle id*/, std::unique_ptr<FrameContext>>;

	public:
		Renderer();
		~Renderer();

		[[nodiscard]] inline GfxDevice& GetDevice() noexcept { return *m_device; }
		[[nodiscard]] inline const GfxDevice& GetDevice() const noexcept { return *m_device; }
		[[nodiscard]] inline RenderWindow* GetRenderWindow(RenderWindowHandle h) noexcept { return m_renderWindows.Get(h); }
		[[nodiscard]] inline const RenderWindow* GetRenderWindow(RenderWindowHandle h) const noexcept { return m_renderWindows.Get(h); }

		std::pair<RenderWindowHandle, RenderWindow*> AddWindow(WindowHandle handle, bool vsync);  // NOTE: The pointer is not safe to store, but okay for temporary actions
		RenderWindowHandle FindWindow(WindowHandle w) const noexcept;
		void RemoveWindow(WindowHandle handle);

		[[nodiscard]] CommandList* GetCommandList(RenderWindowHandle h) noexcept;

		void BeginFrame();
		void EndFrame();

	private:
		void OnDeviceLost() override;
		void OnDeviceRestored() override;
		FrameContext& GetOrCreateFrameContext(RenderWindowHandle h);

	private:
		std::unique_ptr<GfxDevice> m_device;
		CommandQueue               m_graphicsQueue;
		HandlePool<RenderWindow>   m_renderWindows;
		FrameMap                   m_frameContexts;
		WindowMap                  m_windowMap;
		u32                        m_frameSlot = 0;
	};
}
