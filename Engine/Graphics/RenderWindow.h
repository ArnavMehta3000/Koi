#pragma once
#include "Graphics/RHI/DX12/DX12Common.h"

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
