#pragma once
#include "Graphics/RHI/RHI.h"
#include "Graphics/RHI/DX12/DX12.h"

namespace koi::rhi
{
	class DX12SwapChain : public ISwapChain
	{
	public:
		using NativeType = dxgi::SwapChain;

		DX12SwapChain(DX12Device& device, Window& window);
		Object GetNativeType() const override { return m_swapChain.Get(); }

		u32 GetWidth() const { return m_width; }
		u32 GetHeight() const { return m_height; }
		void Present() override;
		void Resize(u32 width, u32 height) override;
		
	private:
		ComPtr<dxgi::SwapChain> m_swapChain;
		HWND m_hWnd;
		u32 m_width;
		u32 m_height;
	};
}
