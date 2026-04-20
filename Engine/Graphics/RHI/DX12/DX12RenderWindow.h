#pragma once
#include "Graphics/RHI/RHI.h"
#include "Graphics/RHI/DX12/DX12SwapChain.h"

namespace koi::rhi
{
	class DX12RenderWindow : public IRenderWindow
	{
	public:
		DX12RenderWindow(DX12Device& device, Window& window);
		~DX12RenderWindow() override;

		ISwapChain* GetSwapChain() const override;
		void Resize(u32 width, u32 height) override;
		void Present() override;

	private:
		DX12SwapChain m_swapChain;
		DX12Device*   m_device;
		bool          m_tearingSupported;
		bool          m_vsyncEnabled;
	};
}
