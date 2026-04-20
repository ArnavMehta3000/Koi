// Contains common RHI types and base interfaces
#pragma once
#include "Core/Core.h"
#include "Core/Object.h"
#include <memory>

namespace koi{ class Window; }

namespace koi::rhi
{
	class IDevice;
	class ISwapChain;
	class IRenderWindow;

	static constexpr u64 FRAME_IN_FLIGHT = 3;

	enum class API { Headless, DX12 };

	inline std::string_view ToString(API api)
	{
		switch (api)
		{
		case API::Headless: return "Headless";
		case API::DX12:     return "DX12";
		default:            return "<Unknown>";
		}
	}

	class IResource
	{
	public:
		virtual ~IResource() = default;

		virtual Object GetNativeType() const = 0;
	};

	class ISwapChain : public IResource
	{
	public:
		virtual ~ISwapChain() = default;

		virtual void Present() = 0;
		virtual void Resize(u32 width, u32 height) = 0;
	};

	class IRenderWindow
	{
	public:
		IRenderWindow(Window& window) : m_window(&window) {}
		virtual ~IRenderWindow() {}

		[[nodiscard]] inline Window* GetWindow() const { return m_window; }

		virtual u32 GetWidth() const = 0;
		virtual u32 GetHeight() const = 0;
		virtual ISwapChain* GetSwapChain() const = 0;
		virtual void Resize(u32 width, u32 height) = 0;
		virtual void Present() = 0;

	protected:
		Window* m_window;
	};

	struct IDeviceNotify
	{
		virtual void OnDeviceLost() = 0;
		virtual void OnDeviceRestored() = 0;
	};

	class IDevice
		: private NonCopyable
		, private NonMovable
		, public IResource
	{
	public:
		virtual ~IDevice() = default;

		// TODO: add swapchain description
		virtual std::unique_ptr<IRenderWindow> CreateRenderWindow(Window& window) = 0;
	};
}
