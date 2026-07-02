#pragma once
#include "Core/Types.h"

namespace koi
{
	class Window;
	struct AppCreateDesc;

	class IAppHost
	{
	public:
		virtual ~IAppHost() = default;

		[[nodiscard]] virtual const AppCreateDesc& GetDesc() const = 0;
		virtual void SetWindow(Window& window) = 0;

		virtual void HostInit() = 0;
		virtual void HostTick(f32 dt) = 0;
		virtual void HostRender() = 0;
		virtual void HostShutdown() = 0;
	};
}
