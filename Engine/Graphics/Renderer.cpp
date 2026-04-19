#include "Graphics/Renderer.h"
#include "Core/Logger.h"

#if KRN_OS_WINDOWS
	#include "Graphics/RHI/DX12/DX12Device.h"
#endif

namespace koi
{
	Renderer::Renderer(gfx::API api)
		: m_api(api)
	{
		switch (api)
		{
		case koi::gfx::API::Headless:
			KOI_VERIFY_MSG(false, "Headless API is not supported yet.");
			break;
		case koi::gfx::API::DX12:
			m_device = std::make_unique<gfx::DX12Device>();
			break;
		}
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::OnDeviceLost()
	{
		KOI_LOG_CRITICAL("Graphics device (API: {}) lost", ToString(m_api));
	}

	void Renderer::OnDeviceRestored()
	{
		KOI_LOG_CRITICAL("Graphics device (API: {}) restored", ToString(m_api));
	}
}
