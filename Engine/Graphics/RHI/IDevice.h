// Contains the base graphics device class
#pragma once
#include "Graphics/RHI/RHI.h"

namespace koi::gfx
{
	struct IDeviceNotify
	{
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};

	class IDevice : NonCopyable, NonMovable
	{
	public:
		virtual ~IDevice() = default;
	};
}
