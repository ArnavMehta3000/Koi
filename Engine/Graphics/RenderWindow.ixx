module;
#include "Platform/Win32.h"
#include <wrl/client.h>
#define D3DX12_USE_ARRAY_COM_PTR
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
export module Koi.Graphics.RenderWindow;

import Koi.Core.Types;
import Koi.Graphics.Types;
import Koi.Graphics.Format;
import Koi.Graphics.RHI.DX12Common;

export namespace koi
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

		[[nodiscard]] Window* GetWindow()              const noexcept { return m_window;            }
		[[nodiscard]] u32     GetWidth()               const noexcept { return m_width;             }
		[[nodiscard]] u32     GetHeight()              const noexcept { return m_height;            }
		[[nodiscard]] u32     GetCurrentBackBufferIndex() const noexcept { return m_currentBackBuffer; }
		[[nodiscard]] Format  GetFormat()              const noexcept { return m_format;            }
		[[nodiscard]] bool    IsVSyncEnabled()         const noexcept { return m_vsync;             }
		[[nodiscard]] TextureHandle CurrentBackBuffer() const noexcept { return m_backBuffers[m_currentBackBuffer]; }
		[[nodiscard]] TextureHandle GetHDRColorBuffer() const noexcept { return m_hdrColorBuffer; }

		void Resize(u32 width, u32 height);
		void Present();
		void SetVSync(bool enabled) noexcept { m_vsync = enabled; }

	private:
		void CreateSwapChain();
		void AcquireBackBuffers();
		void ReleaseBackBuffers();
		void CreateHDRBuffer();
		void ReleaseHDRBuffer();
		void WaitForCurrentBackBuffer();

	private:
		GfxDevice*              m_device = nullptr;
		Window*                 m_window = nullptr;
		HWND                    m_hwnd   = nullptr;
		ComPtr<dxgi::SwapChain> m_swapChain;
		FrameArray<TextureHandle> m_backBuffers{};
		TextureHandle           m_hdrColorBuffer{};
		bool                    m_vsync             = true;
		bool                    m_tearingAllowed    = false;
		FrameArray<u64>         m_frameFenceValues{};
		u32                     m_width             = 0;
		u32                     m_height            = 0;
		u32                     m_currentBackBuffer = 0;
		Format                  m_format            = Format::R8G8B8A8_UNORM;
	};
}
