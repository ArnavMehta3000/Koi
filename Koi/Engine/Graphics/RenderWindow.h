#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/RHI/DX12Common.h"

namespace koi
{
	class GfxDevice;
	class Window;

	class RenderWindow
	{
	public:
		RenderWindow(GfxDevice& device, Window& window, bool vsync);
		~RenderWindow();
		RenderWindow(RenderWindow&& other) noexcept;
		RenderWindow& operator=(RenderWindow&& other) noexcept;

		[[nodiscard]] inline Window* GetWindow() const noexcept{ return m_window; }
		[[nodiscard]] inline u32 GetWidth() const noexcept{ return m_width; }
		[[nodiscard]] inline u32 GetHeight() const noexcept{ return m_height; }
		[[nodiscard]] inline u32 GetCurrentBackBufferIndex() const noexcept{ return m_currentBackBuffer; }
		[[nodiscard]] inline Format GetFormat() const noexcept{ return m_format; }
		[[nodiscard]] inline bool IsVSyncEnabled() const noexcept{ return m_vsync; }
		[[nodiscard]] inline TextureHandle CurrentBackBuffer() const noexcept { return m_backBuffers[m_currentBackBuffer]; }
		[[nodiscard]] inline TextureHandle GetHDRColorBuffer() const noexcept { return m_hdrColorBuffer; }

		// Functions to delay the actual resize until Win32 WM_SIZE pump ends
		void QueueResize(u32 width, u32 height) noexcept;
		void ApplyPendingResize();

		void Present();

		void SetVSync(bool enabled) noexcept { m_vsync = enabled; }

	private:
		void Resize(u32 width, u32 height);
		void CreateSwapChain();
		void AcquireBackBuffers();
		void ReleaseBackBuffers();
		void CreateHDRBuffer();
		void ReleaseHDRBuffer();
		void WaitForCurrentBackBuffer();

	private:
		GfxDevice*                 m_device = nullptr;
		Window*                    m_window = nullptr;
		HWND                       m_hwnd   = nullptr;
		ComPtr<dxgi::SwapChain>    m_swapChain;
		FrameArray<TextureHandle>  m_backBuffers{};
		TextureHandle              m_hdrColorBuffer{};
		bool                       m_vsync             = true;
		bool                       m_tearingAllowed    = false;
		FrameArray<u64>            m_frameFenceValues{};
		u32                        m_width             = 0;
		u32                        m_height            = 0;
		u32                        m_pendingWidth      = 0;  // 0 => nothing latched
		u32                        m_pendingHeight     = 0;
		u32                        m_currentBackBuffer = 0;
		Format                     m_format            = Format::R8G8B8A8_UNORM;
	};
}
