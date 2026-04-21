#include "Graphics/Renderer.h"
#include "Graphics/RHI/Device.h"
#include "Core/Assert.h"
#include "Engine/Engine.h"
#include "Core/Logger.h"

namespace koi
{
	Renderer::Renderer()
	{
		m_device = std::make_unique<GfxDevice>(
		GfxDevice::Desc 
		{
			.EnableDebugLayer = KOI_DEBUG,
			.EnableGPUValidation = KOI_DEBUG
		});

		m_device->SetDeviceNotify(this);

		KOI_LOG_INFO("Initializing renderer");
	}

	Renderer::~Renderer()
	{
		if (m_device)
			m_device->WaitForIdle();

		m_renderWindows.Clear();  // destroys RenderWindows before the device
		m_windowMap.clear();
		m_device.reset();

		KOI_LOG_INFO("Shutdown renderer");
	}

	RenderWindowHandle Renderer::AddWindow(WindowHandle handle, bool vsync)
	{
		if (auto it = m_windowMap.find(handle); it != m_windowMap.end())
			return it->second;

		Window* w = Engine::Get().GetWindowManager().Get(handle);
		KOI_VERIFY_MSG(w, "Trying to create RenderWindow from invalid window handle");
		if (!w) { return {}; }

		RenderWindowHandle h = m_renderWindows.Emplace(*m_device, *w, vsync);
		m_windowMap.emplace(handle, h);

		KOI_LOG_INFO("Created RenderWindow (Handle:{})", h.GetId());
		return h;
	}

	RenderWindowHandle Renderer::FindWindow(WindowHandle w) const noexcept
	{
		auto it = m_windowMap.find(w);
		return it != m_windowMap.end() ? it->second : RenderWindowHandle{};
	}

	void Renderer::RemoveWindow(WindowHandle handle)
	{
		auto it = m_windowMap.find(handle);
		if (it == m_windowMap.end()) return;

		// RenderWindow's destructor calls WaitForIdle, so it's safe to erase here.
		KOI_LOG_INFO("Destroying RenderWindow (Handle:{})", it->second.GetId());
		m_renderWindows.Erase(it->second);
		m_windowMap.erase(it);
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
