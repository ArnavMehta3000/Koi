#include "Graphics/RHI/DX12/DX12Device.h"
#include "Graphics/RHI/DX12/DX12RenderWindow.h"
#include "Platform/Window.h"
#include "Core/StringUtils.h"
#include "Core/Logger.h"
#if KRN_DEBUG
	#include <dxgidebug.h>
#endif
#include <array>

namespace koi
{
	namespace
	{
		static constexpr bool ENABLE_DEBUG_LAYER = true;  // Only used in debug builds
		static constexpr bool ENABLE_VALDATION = true;    // Only used in debug builds
		static constexpr D3D_FEATURE_LEVEL MIN_FEATURE_LEVEL = D3D_FEATURE_LEVEL_11_0;

		void GetAdapter(ComPtr<dxgi::Factory> factory, IDXGIAdapter1** ppAdapter)
		{
			*ppAdapter = nullptr;

			ComPtr<IDXGIAdapter1> adapter;

			ComPtr<IDXGIFactory6> factory6;
			HRESULT hr = factory.As(&factory6);
			if (SUCCEEDED(hr))
			{
				for (UINT adapterIndex = 0;
					SUCCEEDED(factory6->EnumAdapterByGpuPreference(
						adapterIndex,
						DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
						IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
						adapterIndex++)
				{
					DXGI_ADAPTER_DESC1 desc;
					KOI_VERIFY(SUCCEEDED(adapter->GetDesc1(&desc)));

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						continue;  // Don't select the Basic Render Driver adapter
					}

					// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
					if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), MIN_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr)))
					{
						const wchar_t* name = desc.Description;  // For string_view
						const std::string descStr = StringUtils::FromWide(name);
						KOI_LOG_INFO("Direct3D Adapter: VID:{:04X}, PID:{:04X} - {}", desc.VendorId, desc.DeviceId, descStr);
						break;
					}
				}
			}

			if (!adapter)
			{
				for (UINT adapterIndex = 0;
					SUCCEEDED(factory->EnumAdapters1(
						adapterIndex,
						adapter.ReleaseAndGetAddressOf()));
						++adapterIndex)
				{
					DXGI_ADAPTER_DESC1 desc;
					KOI_VERIFY(SUCCEEDED(adapter->GetDesc1(&desc)));

					if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
					{
						continue;  // Don't select the Basic Render Driver adapter
					}

					// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
					if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), MIN_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr)))
					{
						const wchar_t* name = desc.Description;  // For string_view
						const std::string descStr = StringUtils::FromWide(name);
						KOI_LOG_INFO("Direct3D Adapter: VID:{:04X}, PID:{:04X} - {}", desc.VendorId, desc.DeviceId, descStr);break;
					}
				}
			}

			KOI_VERIFY_MSG(adapter, "No Direct3D 12 device found");

			*ppAdapter = adapter.Detach();
		}
	}

	DX12Device::DX12Device()
		: m_notify(nullptr)
		, m_window(nullptr)
	{
		CreateDeviceResources();
	}

	DX12Device::~DX12Device()
	{
		m_dxgiFactory = nullptr;
		m_device = nullptr;
	}

	std::unique_ptr<IRenderWindow> DX12Device::CreateRenderWindow(Window& window)
	{
		return std::make_unique<DX12RenderWindow>(*this, window);
	}

	void DX12Device::CreateDeviceResources()
	{
		u32 dxgiFactoryFlags = 0;

		// --- Debug layer ---
#if KOI_DEBUG
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
		else
		{
			OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
		}

		ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
		if (SUCCEEDED(::DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
		{
			dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

			DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
			{
				80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
			};
			DXGI_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs        = static_cast<UINT>(std::size(hide));
			filter.DenyList.pIDList       = hide;
			dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
		}
#endif

		// --- Create factory ---
		HRESULT hr = ::CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory));
		KOI_ASSERT(SUCCEEDED(hr));

		// Check for tearing support
		{
			BOOL allowTearing = FALSE;

			ComPtr<IDXGIFactory5> factory5;
			if (SUCCEEDED(m_dxgiFactory.As(&factory5)))
			{
				hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
			}

			if (FAILED(hr) || !allowTearing)
			{
				KOI_LOG_WARN("VSync is not enabled");
			}
			m_supportsTearing = allowTearing == TRUE;
		}

		// --- Adapter ---
		ComPtr<IDXGIAdapter1> adapter;
		GetAdapter(m_dxgiFactory, adapter.GetAddressOf());

		hr = ::D3D12CreateDevice(
			adapter.Get(),
			MIN_FEATURE_LEVEL,
			IID_PPV_ARGS(&m_device)
		);
		KOI_ASSERT(SUCCEEDED(hr));


#if KOI_DEBUG
		// Configure debug device (if active).
		ComPtr<ID3D12InfoQueue> d3dInfoQueue;
		if (SUCCEEDED(m_device.As(&d3dInfoQueue)))
		{
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

			D3D12_MESSAGE_ID hide[] =
			{
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
				// Workarounds for debug layer issues on hybrid-graphics systems
				D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			};
			D3D12_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs         = static_cast<UINT>(std::size(hide));
			filter.DenyList.pIDList        = hide;
			d3dInfoQueue->AddStorageFilterEntries(&filter);
		}
#endif

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(m_cmdQueue.ReleaseAndGetAddressOf()));
		KOI_ASSERT(SUCCEEDED(hr));
		m_cmdQueue->SetName(L"DX12 Command Queue");

	}
}
