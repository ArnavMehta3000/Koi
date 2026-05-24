#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Memory/MemoryTracker.h"
#include "Core/Profiling.h"
#include "Platform/Window.h"
#include "Core/Logger.h"
#include <cassert>

namespace koi
{
	namespace
	{
		constexpr Format SWAPCHAIN_FORMAT = Format::R8G8B8A8_UNORM;
		constexpr Format HDR_FORMAT       = Format::R16G16B16A16_FLOAT;
	}

	RenderWindow::RenderWindow(GfxDevice& device, Window& window, bool vsync)
		: m_device(&device)
		, m_window(&window)
		, m_hwnd(static_cast<HWND>(window.GetNativeHandle()))
		, m_vsync(vsync)
		, m_tearingAllowed(device.SupportsTearing())
		, m_width(window.GetWidth())
		, m_height(window.GetHeight())
		, m_format(SWAPCHAIN_FORMAT)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		assert(m_hwnd && "Window has no native HWND");
		CreateSwapChain();
		AcquireBackBuffers();
		CreateHDRBuffer();
	}

	RenderWindow::~RenderWindow()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (!m_device) return;   // moved-from

		m_device->WaitForIdle();
		ReleaseHDRBuffer();
		ReleaseBackBuffers();
		m_swapChain = nullptr;
	}

	RenderWindow::RenderWindow(RenderWindow&& other) noexcept
		: m_device(other.m_device)
		, m_window(other.m_window)
		, m_hwnd(other.m_hwnd)
		, m_swapChain(std::move(other.m_swapChain))
		, m_backBuffers(other.m_backBuffers)
		, m_hdrColorBuffer(other.m_hdrColorBuffer)
		, m_frameFenceValues(other.m_frameFenceValues)
		, m_width(other.m_width)
		, m_height(other.m_height)
		, m_currentBackBuffer(other.m_currentBackBuffer)
		, m_format(other.m_format)
		, m_vsync(other.m_vsync)
		, m_tearingAllowed(other.m_tearingAllowed)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		other.m_device = nullptr;
		other.m_window = nullptr;
		other.m_hwnd   = nullptr;

		other.m_backBuffers.fill(TextureHandle{});
		other.m_hdrColorBuffer = TextureHandle{};
	}

	RenderWindow& RenderWindow::operator=(RenderWindow&& other) noexcept
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (this != &other)
		{
			// Release our resources first
			if (m_device)
			{
				m_device->WaitForIdle();
				ReleaseBackBuffers();
				m_swapChain.Reset();
			}

			m_device            = other.m_device;
			m_window            = other.m_window;
			m_hwnd              = other.m_hwnd;
			m_swapChain         = std::move(other.m_swapChain);
			m_backBuffers       = other.m_backBuffers;
			m_hdrColorBuffer    = other.m_hdrColorBuffer;
			m_frameFenceValues  = other.m_frameFenceValues;
			m_width             = other.m_width;
			m_height            = other.m_height;
			m_currentBackBuffer = other.m_currentBackBuffer;
			m_format            = other.m_format;
			m_vsync             = other.m_vsync;
			m_tearingAllowed    = other.m_tearingAllowed;

			other.m_device = nullptr;
			other.m_window = nullptr;
			other.m_hwnd   = nullptr;

			other.m_backBuffers.fill(TextureHandle{});
			other.m_hdrColorBuffer = TextureHandle{};
		}
		return *this;
	}

	void RenderWindow::Resize(u32 width, u32 height)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (width == 0 || height == 0) return;
		if (width == m_width && height == m_height) return;
		assert(m_device && "Render Window has been moved-from");

		KOI_LOG_DEBUG("Resizing RenderWindow ({}x{}) -> ({}x{})",
			m_width, m_height, width, height);

		m_device->WaitForIdle();
		ReleaseHDRBuffer();
		ReleaseBackBuffers();
		m_device->FlushPendingDeletions();

		const u32 flags = m_tearingAllowed ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		DX_CHECK(m_swapChain->ResizeBuffers(
			BACK_BUFFER_COUNT, width, height, ToDXGIFormat(SWAPCHAIN_FORMAT), flags));

		m_width             = width;
		m_height            = height;
		m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();

		AcquireBackBuffers();
		CreateHDRBuffer();

		KOI_LOG_TRACE("RenderWindow resize successful");
	}

	void RenderWindow::Present()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		const u32 syncInterval = m_vsync ? 1 : 0;
		const u32 flags        = (!m_vsync && m_tearingAllowed) ? DXGI_PRESENT_ALLOW_TEARING : 0;

		HRESULT hr = m_swapChain->Present(syncInterval, flags);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			KOI_LOG_ERROR("Present returned device-removed (0x{:08X})", static_cast<u32>(hr));
			m_device->HandleDeviceLost();
			return;
		}
		DX_CHECK(hr);

		/// DevNote: Here are the steps done regarding synchronization
		/// - After presenting buffer N, record the fence value at that moment
		/// - Rotate to the next buffer the swapchain gives
		/// - Before writing to 'that' buffer, wait for the fence value recorded the 'last time' that slot was used
		/// This guarantees I never start writing to a backbuffer the GPU is still presenting too

		m_frameFenceValues[m_currentBackBuffer] = m_device->SignalFrame();
		m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
		WaitForCurrentBackBuffer();
	}

	void RenderWindow::CreateSwapChain()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		DXGI_SWAP_CHAIN_DESC1 desc
		{
			.Width       = m_width,
			.Height      = m_height,
			.Format      = ToDXGIFormat(m_format),
			.SampleDesc  = { 1, 0 },
			.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
			.BufferCount = BACK_BUFFER_COUNT,
			.Scaling     = DXGI_SCALING_STRETCH,
			.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD,
			.AlphaMode   = DXGI_ALPHA_MODE_IGNORE,
			.Flags       = m_tearingAllowed ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0u,
		};

		// [TODO]: Handle fullscreen description

		dxgi::Factory* factory = m_device->GetDXGIFactory();


		ComPtr<IDXGISwapChain1> sc1;
		DX_CHECK(factory->CreateSwapChainForHwnd(
			m_device->GetGraphicsQueue(), m_hwnd, &desc, nullptr, nullptr, sc1.GetAddressOf()));

		// No Alt+Enter to fullscreen
		DX_CHECK(factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER));

		DX_CHECK(sc1.As(&m_swapChain));
		m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
	}

	void RenderWindow::AcquireBackBuffers()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		TextureDesc desc
		{
			.Width  = m_width,
			.Height = m_height,
			.Format = m_format,
			.Usage  = TextureUsage::RenderTarget | TextureUsage::Present
		};

		for (u32 i = 0; i < BACK_BUFFER_COUNT; i++)
		{
			ComPtr<dx12::Resource> buffer;
			DX_CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer)));

			buffer->SetName(fmt::format(L"Backbuffer {}", i).c_str());

			TextureHandle h = m_device->RegisterSwapChainImage(buffer, desc);
			m_backBuffers[i] = h;

			// Create RTV
			if (Texture* tex = m_device->Get(h))
			{
				DescriptorHeap& rtvHeap = m_device->RTVHeap();
				tex->RTVIndex = rtvHeap.Allocate();

				D3D12_RENDER_TARGET_VIEW_DESC rtvDesc
				{
					.Format        = ToDXGIFormat(SWAPCHAIN_FORMAT),
					.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D
				};

				m_device->GetNative()->CreateRenderTargetView(
					tex->Resource.Get(),
					&rtvDesc,
					rtvHeap.CpuHandle(tex->RTVIndex));
			}
		}
	}

	void RenderWindow::ReleaseBackBuffers()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		for (u32 i = 0; i < BACK_BUFFER_COUNT; ++i)
		{
			if (!m_backBuffers[i]) continue;

			if (Texture* tex = m_device->Get(m_backBuffers[i]))
			{
				m_device->RTVHeap().Free(tex->RTVIndex);
				tex->RTVIndex = INVALID_DESCRIPTOR;

				// Release ref here since, Device also holds a ref
				// So when we call GfxDevice::Destroy() to remove from pool -> eventually destroying the resource
				tex->Resource.Reset();
			}

			// Erase immediately (we already waited for idle in the caller)
			// NOTE: bypassing the deferred-deletion queue is safe here because
			// we WaitForIdle() before calling ReleaseBackBuffers().
			// Accessing Destroy() would defer and that's also OK, but iterating
			// is cleaner.
			// However, to keep things simple + consistent, we go through Destroy
			// and flush via WaitForIdle in Resize.
			m_device->Destroy(m_backBuffers[i]);
			m_backBuffers[i] = TextureHandle{};
		}
	}

	void RenderWindow::CreateHDRBuffer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_hdrColorBuffer = m_device->CreateTexture(TextureDesc
		{
			.Width     = m_width,
			.Height    = m_height,
			.Format    = HDR_FORMAT,
			.Usage     = TextureUsage::RenderTarget | TextureUsage::ShaderRead,
			.DebugName = "HDR Color Buffer",
		});
	}

	void RenderWindow::ReleaseHDRBuffer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (m_hdrColorBuffer)
		{
			m_device->Destroy(m_hdrColorBuffer);
			m_hdrColorBuffer = {};
		}
	}

	void RenderWindow::WaitForCurrentBackBuffer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		const u64 value = m_frameFenceValues[m_currentBackBuffer];
		if (value == 0)
			return;

		m_device->WaitForFence(value);
	}
}
