#include "Graphics/RHI/DX12/DX12SwapChain.h"
#include "Graphics/RHI/DX12/DX12Device.h"
#include "Platform/Window.h"

namespace koi::rhi
{
	DX12SwapChain::DX12SwapChain(DX12Device& device, Window& window)
		: m_width(window.GetWidth())
		, m_height(window.GetHeight())
		, m_hWnd(static_cast<HWND>(window.GetNativeHandle()))
	{
		KOI_ASSERT_MSG(m_hWnd != nullptr, "Invalid window handle");
	}

	void DX12SwapChain::Present()
	{
	}

	void DX12SwapChain::Resize(u32 width, u32 height)
	{
	}
}
