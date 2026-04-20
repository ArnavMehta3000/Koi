#include "Graphics/Renderer.h"
#include "Core/Assert.h"
#include "Engine/Engine.h"
#include "Core/Logger.h"

#if KRN_OS_WINDOWS
	#include "Graphics/RHI/DX12/DX12Device.h"
#endif

namespace koi
{
	using namespace koi::rhi;

	Renderer::Renderer()
	{
	}

	Renderer::~Renderer()
	{
		m_device = nullptr;
	}

	RenderWindowHandle Renderer::AddWindow(WindowHandle handle)
	{
		KOI_ASSERT_MSG(m_windowMap.find(handle) == m_windowMap.end(), "Window already has a render window!");

		Window* window = Engine::Get().GetWindowManager().Get(handle);
		KOI_ASSERT(window);
		if (!window) return;

		KOI_ASSERT(m_device);
		RenderWindowHandle renderHandle/* = m_renderWindows.Emplace(
			m_device->CreateRenderWindow(*window))*/;

		m_windowMap[handle] = renderHandle;
		return renderHandle;
	}

	void Renderer::RemoveWindow(WindowHandle handle)
	{
		//TODO: yet to implement (needs to flush GPU)
	}

	void Renderer::BeginFrame()
	{
	}

	void Renderer::EndFrame()
	{
	}
}
