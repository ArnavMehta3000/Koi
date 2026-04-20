#pragma once
#include "Graphics/RHI/DX12/DX12Common.h"

namespace koi
{
	class DX12Device final : public IDevice
	{
	public:
		using NativeType = dx12::Device;

		DX12Device();
		~DX12Device();

		Object GetNativeType() const override { return m_device.Get(); }
		dxgi::Factory* GetDXGIFactory() const { return m_dxgiFactory.Get(); }

		std::unique_ptr<IRenderWindow> CreateRenderWindow(Window& window) override;

	private:
		void CreateDeviceResources();

	private:
		IDeviceNotify* m_notify;

		ComPtr<dxgi::Factory>      m_dxgiFactory;
		ComPtr<dx12::Device>       m_device;
		ComPtr<dx12::CommandList>  m_cmdList;
		ComPtr<dx12::CommandQueue> m_cmdQueue;

		HWND m_window;
		CD3DX12FeatureSupport m_featureSupport;
		bool m_supportsTearing = false;
	};
}
