#pragma once
#include "Graphics/RHI/IDevice.h"
#include <memory>

namespace koi
{
	class Renderer
	{
	public:

	private:
		std::unique_ptr<IDevice> m_device;
	};
}
