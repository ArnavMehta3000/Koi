#pragma once
#include "Graphics/Types.h"
#include "Graphics/DX12/DX12Common.h"

namespace koi
{
	class GfxDevice;
	class Window;

	class RenderWindow
	{
	public:
		static constexpr u32 BACK_BUFFER_COUNT = 3;

		RenderWindow(GfxDevice& device, Window& window, bool vsync);
    	~RenderWindow();
    	RenderWindow(RenderWindow&&) noexcept;
    	RenderWindow& operator=(RenderWindow&&) noexcept;

     	void Resize(u32 width, u32 height);
        void Present();

		[[nodiscard]] Window* GetWindow() const noexcept { return m_window; }
		[[nodiscard]] u32 GetWidth()  const noexcept { return m_width; }
		[[nodiscard]] u32 GetHeight() const noexcept { return m_height; }
		[[nodiscard]] u32 GetCurrentBackBufferIndex() const noexcept { return m_currentBackBuffer; }
		[[nodiscard]] TextureHandle CurrentBackBuffer() const noexcept;
		[[nodiscard]] Format        GetFormat() const noexcept { return m_format; }
		[[nodiscard]] bool          IsVSyncEnabled() const noexcept { return m_vsync; }

		void SetVSync(bool enabled) noexcept { m_vsync = enabled; }

	private:
		void CreateSwapChain();
		void CreateBackBufferViews();
		void ReleaseBackBuffers();

	private:
		GfxDevice*              m_device = nullptr;
		Window*                 m_window = nullptr;
		HWND                    m_hWnd = nullptr;
		ComPtr<dxgi::SwapChain> m_swapChain;

		// TODO: Texture handles
		ComPtr<ID3D12Resource> m_backBuffers[BACK_BUFFER_COUNT] = {};
		u64 m_frameFenceValues[BACK_BUFFER_COUNT] = {};

		u32  m_width = 0, m_height = 0;
		bool m_vsync = false;
		bool m_tearingSupported = false;
	};
}
