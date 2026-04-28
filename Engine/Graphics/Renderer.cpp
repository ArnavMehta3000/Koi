#include "Graphics/Renderer.h"
#include "Graphics/RHI/Device.h"
#include "Core/Assert.h"
#include "Engine/Engine.h"
#include "Core/Logger.h"
#include <DirectXColors.h>

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
		m_graphicsQueue = CommandQueue(m_device->GetGraphicsQueue());

		KOI_LOG_INFO("Initializing renderer");
	}

	Renderer::~Renderer()
	{
		if (m_device)
			m_device->WaitForIdle();

		m_frameContexts.clear();
		m_renderWindows.Clear();  // destroys RenderWindows before the device
		m_windowMap.clear();
		m_device = nullptr;

		KOI_LOG_INFO("Shutdown renderer");
	}

	std::pair<RenderWindowHandle, RenderWindow*> Renderer::AddWindow(WindowHandle handle, bool vsync)
	{
		if (auto it = m_windowMap.find(handle); it != m_windowMap.end())
			return { it->second, GetRenderWindow(it->second) };

		Window* w = Engine::Get().GetWindowManager().GetWindow(handle);

		KOI_VERIFY_MSG(w, "Trying to create RenderWindow from invalid window handle");
		if (!w) { return { RenderWindowHandle{}, nullptr }; }

		RenderWindowHandle h = m_renderWindows.Emplace(*m_device, *w, vsync);
		m_windowMap.emplace(handle, h);

		KOI_LOG_TRACE("Created RenderWindow (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		return { h, GetRenderWindow(h) };
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
		KOI_LOG_TRACE("Destroying RenderWindow (Handle: idx={}, gen={})", handle.GetIndex(), handle.GetGeneration());
		m_renderWindows.Erase(it->second);
		m_windowMap.erase(it);
	}

	void Renderer::BeginFrame()
	{
		// Open a command list for each RenderWindow and record a clear.
		m_renderWindows.ForEach([this](RenderWindowHandle h, RenderWindow& rw)
		{
			FrameContext& ctx = GetOrCreateFrameContext(h);

			ctx.List.Begin(m_frameSlot);

			TextureHandle bb = rw.CurrentBackBuffer();
			ctx.List.TransitionBarrier(bb, D3D12_RESOURCE_STATE_RENDER_TARGET);
			ctx.List.ClearRenderTarget(bb, DirectX::Colors::Olive.f);
		});
	}

	void Renderer::EndFrame()
	{
		// Close each window's command list, transition back to PRESENT, submit, then present.
		m_renderWindows.ForEach([this](RenderWindowHandle h, RenderWindow& rw)
		{
			FrameContext& ctx = GetOrCreateFrameContext(h);

			ctx.List.TransitionBarrier(rw.CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
			ctx.List.End();

			m_graphicsQueue.Submit(ctx.List);
			rw.Present();  // signals the frame fence internally
		});

		m_frameSlot = (m_frameSlot + 1) % FRAMES_IN_FLIGHT;
	}

	CommandList* Renderer::GetCommandList(RenderWindowHandle h) noexcept
	{
		auto it = m_frameContexts.find(h.GetId());
		return it != m_frameContexts.end() ? &it->second->List : nullptr;
	}

	void Renderer::OnDeviceLost()
	{
		KOI_LOG_CRITICAL(" Graphics device lost");
	}

	void Renderer::OnDeviceRestored()
	{
		KOI_LOG_INFO("Graphics device restored");
	}

	Renderer::FrameContext& Renderer::GetOrCreateFrameContext(RenderWindowHandle h)
	{
		auto [it, inserted] = m_frameContexts.try_emplace(h.GetId(), nullptr);
		if (inserted)
		{
			auto ctx = std::make_unique<FrameContext>();

			ctx->Allocator.Init(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT,
				fmt::format(L"Renderer Allocator (RenderWindowHandle: {})", h.GetId()));

			ctx->List.Init(*m_device, ctx->Allocator, D3D12_COMMAND_LIST_TYPE_DIRECT,
				fmt::format(L"Renderer CmdList (RenderWindowHandle: {})", h.GetId()));

			it->second = std::move(ctx);
		}
		return *it->second;
	}
}
