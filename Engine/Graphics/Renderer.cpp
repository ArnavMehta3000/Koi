#include "Graphics/Renderer.h"
#include "Graphics/RHI/Device.h"
#include "Core/Assert.h"
#include "Engine/Engine.h"
#include "Core/Logger.h"

namespace koi
{
	Renderer::Renderer()
	{
		m_device = std::make_unique<GfxDevice>();
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
		// TODO: Reset allocators
	}

	void Renderer::EndFrame()
	{
		m_renderWindows.ForEach([](RenderWindowHandle, RenderWindow& rw)
		{
			rw.Present();
		});
	}

	void Renderer::OnDeviceLost()
	{
		KOI_LOG_CRITICAL(" Graphics device lost");
	}

	void Renderer::OnDeviceRestored()
	{
		KOI_LOG_INFO("Graphics device restored");
	}
}
