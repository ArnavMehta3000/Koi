#include "Graphics/RHI/Device.h"
#include "Core/Logger.h"
#include "Core/StringUtils.h"

#if KOI_DEBUG
	#include <dxgidebug.h>
#endif

namespace koi
{
	// From DX12Common.h
	void ThrowOnHRFail(HRESULT hr, const char* expr, const char* file, int line)
	{
		if (FAILED(hr))
		{
			KOI_LOG_ERROR("D3D12 call failed (0x{:08X}): {} at {}:{}",
				static_cast<u32>(hr), expr, file, line);
			KOI_ASSERT_MSG(false, "D3D12 call failed");
		}
	}

	GfxDevice::GfxDevice(const GfxDevice::Desc& desc)
		: m_desc(desc)
	{
		CreateFactory(desc.EnableDebugLayer);
		PickAdapter();
		CreateDevice();
		CreateQueues();
		CreateFences();
		CreateHeaps();
	}

	GfxDevice::~GfxDevice()
	{
		WaitForIdle();
		FlushDeferredDeletions(~0ull);

		if (m_fenceEvent)
		{
			::CloseHandle(m_fenceEvent);
			m_fenceEvent = nullptr;
		}
	}

	u64 GfxDevice::GetCompletedFenceValue() const noexcept
	{
		return m_frameFence ? m_frameFence->GetCompletedValue() : 0;
	}

	u64 GfxDevice::SignalFrame()
	{
		const u64 value = m_nextFenceValue++;
		DX_CHECK(m_graphicsQueue->Signal(m_frameFence.Get(), value));
		++m_frameIndex;
		FlushDeferredDeletions(m_frameFence->GetCompletedValue());
		return value;
	}

	void GfxDevice::WaitForFence(u64 fenceValue)
	{
		if (m_frameFence->GetCompletedValue() >= fenceValue) return;
		DX_CHECK(m_frameFence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		::WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	void GfxDevice::WaitForIdle()
	{
		const u64 value = m_nextFenceValue++;
		DX_CHECK(m_graphicsQueue->Signal(m_frameFence.Get(), value));
		WaitForFence(value);
	}

	void GfxDevice::HandleDeviceLost()
	{
		KOI_LOG_CRITICAL("D3D12 device lost. Attempting recovery...");

		if (m_notify) m_notify->OnDeviceLost();

		// Drop all GPU resources and rebuild.
		FlushDeferredDeletions(~0ull);
		m_textures.Clear();
		m_buffers.Clear();

		m_graphicsQueue.Reset();
		m_frameFence.Reset();
		m_device.Reset();
		m_adapter.Reset();
		m_dxgiFactory.Reset();

		CreateFactory(m_desc.EnableDebugLayer);
		PickAdapter();
		CreateDevice();
		CreateQueues();
		CreateFences();
		CreateHeaps();

		if (m_notify) m_notify->OnDeviceRestored();
	}

	void GfxDevice::CreateFactory(bool enableDebug)
	{
		u32 flags = 0;

#if KOI_DEBUG
		if (enableDebug)
		{
			ComPtr<ID3D12Debug> debugCtl;
			if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debugCtl))))
			{
				debugCtl->EnableDebugLayer();

				if (m_desc.EnableGPUValidation)
				{
					ComPtr<ID3D12Debug1> debug1;
					if (SUCCEEDED(debugCtl.As(&debug1)))
						debug1->SetEnableGPUBasedValidation(TRUE);
				}
			}
			else
			{
				KOI_LOG_WARN("D3D12 debug layer not available");
			}

			ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
			if (SUCCEEDED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
			{
				flags = DXGI_CREATE_FACTORY_DEBUG;
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
				dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);

				DXGI_INFO_QUEUE_MESSAGE_ID hide[] = { 80 /* swapchain output not controlled */ };
				DXGI_INFO_QUEUE_FILTER filter{};
				filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
				filter.DenyList.pIDList = hide;
				dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
			}
		}
#endif

		DX_CHECK(::CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_dxgiFactory)));

		// Tearing support
		BOOL allowTearing = FALSE;
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(m_dxgiFactory.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
		m_supportsTearing = (allowTearing == TRUE);
		if (!m_supportsTearing)
			KOI_LOG_WARN("Variable refresh rate / tearing not supported");
	}

	void GfxDevice::PickAdapter()
	{
		static constexpr D3D_FEATURE_LEVEL MIN_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

		ComPtr<IDXGIAdapter1> adapter;
		for (u32 i = 0;
			SUCCEEDED(m_dxgiFactory->EnumAdapterByGpuPreference(
				i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
				IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
			++i)
		{
			DXGI_ADAPTER_DESC1 desc{};
			DX_CHECK(adapter->GetDesc1(&desc));

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

			if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), MIN_FEATURE_LEVEL,
				__uuidof(ID3D12Device), nullptr)))
			{
				const std::string name = StringUtils::FromWide(desc.Description);
				KOI_LOG_INFO("D3D12 Adapter selected: VID:{:04X} PID:{:04X} - {}",
					desc.VendorId, desc.DeviceId, name);
				break;
			}
		}

		KOI_VERIFY_MSG(adapter, "No D3D12-capable adapter found");
		DX_CHECK(adapter.As(&m_adapter));
	}

	void GfxDevice::CreateDevice()
	{
		static constexpr D3D_FEATURE_LEVEL MIN_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;
		DX_CHECK(::D3D12CreateDevice(m_adapter.Get(), MIN_FEATURE_LEVEL, IID_PPV_ARGS(&m_device)));
		m_device->SetName(L"Koi GfxDevice");

#if KOI_DEBUG
		ComPtr<ID3D12InfoQueue> infoQueue;
		if (SUCCEEDED(m_device.As(&infoQueue)))
		{
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

			D3D12_MESSAGE_ID hide[] =
			{
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			};
			D3D12_INFO_QUEUE_FILTER filter{};
			filter.DenyList.NumIDs = static_cast<UINT>(std::size(hide));
			filter.DenyList.pIDList = hide;
			infoQueue->AddStorageFilterEntries(&filter);
		}
#endif
	}

	void GfxDevice::CreateQueues()
	{
		D3D12_COMMAND_QUEUE_DESC desc
		{
			.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
			.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE
		};
		DX_CHECK(m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_graphicsQueue)));
		m_graphicsQueue->SetName(L"Koi Graphics Queue");
	}

	void GfxDevice::CreateFences()
	{
		DX_CHECK(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frameFence)));
		m_frameFence->SetName(L"Koi Frame Fence");

		m_fenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
		KOI_VERIFY_MSG(m_fenceEvent, "Failed to create fence event");
	}

	void GfxDevice::CreateHeaps()
	{
		m_rtvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1024, false);
		m_dsvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 256, false);
		m_resourceHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 65536, true);
		m_samplerHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 512, true);
	}

	void GfxDevice::FlushDeferredDeletions(u64 completedFence)
	{
		while (!m_deletionQueue.empty())
		{
			const PendingDeletion& p = m_deletionQueue.front();
			if (p.FenceValue > completedFence) break;

			switch (p.Type)
			{
			case PendingDeletion::Kind::Texture:
				if (Texture* t = m_textures.Get(p.Texture))
				{
					//m_rtvHeap.Free(t->RtvIndex);
					//m_dsvHeap.Free(t->DsvIndex);
					//m_resourceHeap.Free(t->SrvIndex);
					//m_resourceHeap.Free(t->UavIndex);
					// IsSwapChainImage: we must NOT release the ComPtr here — swapchain owns it.
					// The pool erase below will drop the ref, but since the swapchain also holds
					// one it stays alive.
				}
				m_textures.Erase(p.Texture);
				break;

			case PendingDeletion::Kind::Buffer:
				if (Buffer* b = m_buffers.Get(p.Buffer))
				{
					//m_resourceHeap.Free(b->CbvIndex);
					//m_resourceHeap.Free(b->SrvIndex);
					//m_resourceHeap.Free(b->UavIndex);
					//if (b->MappedPtr && b->Resource)
					//	b->Resource->Unmap(0, nullptr);
				}
				m_buffers.Erase(p.Buffer);
				break;
			}
			m_deletionQueue.pop();
		}
	}
}
