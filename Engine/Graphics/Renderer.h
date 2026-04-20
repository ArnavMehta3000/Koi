#pragma once
#include "Graphics/RHI/RHI.h"
#include <memory>

namespace koi
{
	class Window;

	class Renderer: rhi::IDeviceNotify
	{
	public:
		Renderer(rhi::API api);
		~Renderer();

	private:
		std::unique_ptr<rhi::IDevice> m_device;
		rhi::API m_api;

		// Inherited via IDeviceNotify
		void OnDeviceLost() override;
		void OnDeviceRestored() override;
	};
}
