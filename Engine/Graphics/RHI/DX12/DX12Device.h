#pragma once
#include "Graphics/RHI/IDevice.h"
#include "Graphics/RHI/DX12/DX12.h"

namespace koi { class Window; }

namespace koi::gfx
{
	class DX12Device final : public IDevice
	{
	public:
		using NativeType = dx12::Device;

		DX12Device();
		~DX12Device();

	private:
		void CreateDeviceResources();

	private:
		IDeviceNotify* m_notify = nullptr;

		ComPtr<dxgi::Factory> m_dxgiFactory;
		ComPtr<dx12::Device> m_device;
		ComPtr<dx12::CommandList> m_cmdList;
		ComPtr<dx12::CommandQueue> m_cmdQueue;

		HWND m_window;
		CD3DX12FeatureSupport m_featureSupport;
		bool m_supportsTearing = false;
	};
}
