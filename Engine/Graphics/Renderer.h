#pragma once
#include "Graphics/RHI/RHI.h"
#include "Graphics/RHI/IDevice.h"
#include <memory>

namespace koi
{
	class Window;

	class Renderer: gfx::IDeviceNotify
	{
	public:
		Renderer(gfx::API api);
		~Renderer();

	private:
		std::unique_ptr<gfx::IDevice> m_device;
		gfx::API m_api;

		// Inherited via IDeviceNotify
		void OnDeviceLost() override;
		void OnDeviceRestored() override;
	};
}
