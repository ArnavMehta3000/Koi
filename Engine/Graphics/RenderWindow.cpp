#include "Graphics/RenderWindow.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Platform/Window.h"
#include "Core/Logger.h"

namespace koi
{
	namespace
	{
		constexpr Format SWAPCHAIN_FORMAT = Format::R8G8B8A8_UNORM;
	}

	RenderWindow::RenderWindow(GfxDevice& device, Window& window, bool vsync)
		: m_device(&device)
		, m_window(&window)
		, m_hwnd(static_cast<HWND>(window.GetNativeHandle()))
		, m_width(window.GetWidth())
		, m_height(window.GetHeight())
		, m_format(SWAPCHAIN_FORMAT)
		, m_vsync(vsync)
		, m_tearingAllowed(device.SupportsTearing())
	{
		KOI_ASSERT_MSG(m_hwnd, "Window has no native HWND");
		CreateSwapChain();
		AcquireBackBuffers();
	}

	RenderWindow::~RenderWindow()
	{
		if (!m_device) return;   // moved-from

		m_device->WaitForIdle();
		ReleaseBackBuffers();
		m_swapChain = nullptr;
	}

	RenderWindow::RenderWindow(RenderWindow&& other) noexcept
		: m_device(other.m_device)
		, m_window(other.m_window)
		, m_hwnd(other.m_hwnd)
		, m_swapChain(std::move(other.m_swapChain))
		, m_backBuffers(other.m_backBuffers)
		, m_frameFenceValues(other.m_frameFenceValues)
		, m_width(other.m_width)
		, m_height(other.m_height)
		, m_currentBackBuffer(other.m_currentBackBuffer)
		, m_format(other.m_format)
		, m_vsync(other.m_vsync)
		, m_tearingAllowed(other.m_tearingAllowed)
	{
		other.m_device = nullptr;
		other.m_window = nullptr;
		other.m_hwnd   = nullptr;

		other.m_backBuffers.fill(TextureHandle());
	}

	RenderWindow& RenderWindow::operator=(RenderWindow&& other) noexcept
	{
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
		}
		return *this;
	}

	void RenderWindow::Resize(u32 width, u32 height)
	{
		if (width == 0 || height == 0) return;
		if (width == m_width && height == m_height) return;
		KOI_ASSERT_MSG(m_device, "Render Window has been moved-from");

		KOI_LOG_DEBUG("Resizing RenderWindow ({}x{}) -> ({}x{})",
			m_width, m_height, width, height);

		m_device->WaitForIdle();
		ReleaseBackBuffers();

		// Pending swapchain-image destructions are queued; flush them before Resize.
		// WaitForIdle already advanced the fence, but the deletion queue is processed
		// on SignalFrame. We need to force a flush here:
		// (A public FlushDeferredDeletions(u64) on GfxDevice would be cleaner; for now
		// WaitForIdle + SignalFrame-less flush is awkward. One clean option: expose
		// GfxDevice::FlushDestroyed() and call it here.)
		// Workaround: just reset our ComPtrs via ReleaseBackBuffers (done above),
		// and rely on the pool erase dropping the last ref on the next flush.
		// For safety we force-clear pending deletions by signaling & waiting:

		// [TODO]: Fix this
		m_device->SignalFrame();
		m_device->WaitForIdle();

		const u32 flags = m_tearingAllowed ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		DX_CHECK(m_swapChain->ResizeBuffers(
			BACK_BUFFER_COUNT, width, height, ToDXGIFormat(SWAPCHAIN_FORMAT), flags));

		m_width             = width;
		m_height            = height;
		m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();

		AcquireBackBuffers();

		KOI_LOG_TRACE("RenderWindow resize successful");
	}

	void RenderWindow::Present()
	{
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

		// Record the fence value that guards this backbuffer, then rotate.
		m_frameFenceValues[m_currentBackBuffer] = m_device->SignalFrame();
		m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();

		// Before the renderer writes to the *new* current backbuffer, ensure the
		// GPU has finished its previous use of that same backbuffer slot.
		WaitForCurrentBackBuffer();
	}

	void RenderWindow::CreateSwapChain()
	{
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

	void RenderWindow::WaitForCurrentBackBuffer()
	{
		const u64 value = m_frameFenceValues[m_currentBackBuffer];
		if (value == 0)
			return;

		m_device->WaitForFence(value);
	}
}
