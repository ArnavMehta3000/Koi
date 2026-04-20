#include "Graphics/RHI/DX12/DX12RenderWindow.h"
#include "Graphics/RHI/DX12/DX12Device.h"
#include "Platform/Window.h"

namespace koi::rhi
{
	DX12RenderWindow::DX12RenderWindow(DX12Device& device, Window& window)
		: IRenderWindow(window)
		, m_swapChain(device, window)
		, m_device(&device)
		, m_tearingSupported(false)
		, m_vsyncEnabled(false)
	{
	}

	DX12RenderWindow::~DX12RenderWindow()
	{
		m_device = nullptr;
	}

	ISwapChain* DX12RenderWindow::GetSwapChain() const
	{
		return nullptr;
	}

	void DX12RenderWindow::Resize(u32 width, u32 height)
	{
		// TODO: Handle resize
		m_swapChain.Resize(width, height);
	}

	void DX12RenderWindow::Present()
	{
		m_swapChain.Present();
	}
}
