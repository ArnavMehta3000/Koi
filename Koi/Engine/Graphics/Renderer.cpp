#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <cassert>

namespace koi
{
	Renderer::Renderer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_device = std::make_unique<GfxDevice>(GfxDevice::Desc
		{
			.EnableDebugLayer    = KOI_DEBUG,
			.EnableGPUValidation = KOI_DEBUG,
		});

		m_device->SetDeviceNotify(this);
		m_graphicsQueue = CommandQueue(m_device->GetGraphicsQueue());

		m_defaultResources.Init(*m_device);
		KOI_LOG_INFO("Initializing renderer");
	}

	Renderer::~Renderer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (m_device)
			m_device->WaitForIdle();

		m_frameContext.reset();
		if (m_depthTexture)
			m_device->Destroy(m_depthTexture);
		m_renderWindow.reset();
		m_defaultResources.Shutdown(*m_device);
		m_device = nullptr;

		KOI_LOG_INFO("Shutdown renderer");
	}

	void Renderer::InitMainWindow(Window& window, bool vsync, Format depthFormat)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_depthFormat = depthFormat;
		m_renderWindow.emplace(*m_device, window, vsync);

		window.SetResizeCallback([this](u32 w, u32 h)
		{
			if (m_renderWindow) m_renderWindow->Resize(w, h);
			RebuildDepthBuffer(w, h);
		});

		m_frameContext = std::make_unique<FrameContext>();
		m_frameContext->Allocator.Init(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer Allocator");
		m_frameContext->List.Init(*m_device, m_frameContext->Allocator, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer CmdList");

		RebuildDepthBuffer(window.GetWidth(), window.GetHeight());

		KOI_LOG_TRACE("Renderer: main window initialized");
	}

	void Renderer::RebuildDepthBuffer(u32 w, u32 h)
	{
		if (m_depthFormat == Format::UNKNOWN) return;
		if (m_depthTexture) m_device->Destroy(m_depthTexture);
		if (w == 0 || h == 0) return;

		m_depthTexture = m_device->CreateTexture(TextureDesc
		{
			.Width     = w,
			.Height    = h,
			.Format    = m_depthFormat,
			.Usage     = TextureUsage::DepthStencil,
			.DebugName = "SceneDepth",
		});
	}

	void Renderer::BeginFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		if (!m_renderWindow || !m_frameContext) return;

		m_frameGraph.ResetAll();
		m_device->GetUploadContext().FlushQueued(m_device->GetGraphicsQueue());

		m_frameContext->List.Begin(m_frameSlot);

		constexpr f32 kClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_frameContext->List.TransitionBarrier(m_renderWindow->GetHDRColorBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_frameContext->List.ClearRenderTarget(m_renderWindow->GetHDRColorBuffer(), kClearColor);

		if (m_depthTexture)
			m_frameContext->List.ClearDepthStencil(m_depthTexture);
	}

	void Renderer::EndFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		if (!m_renderWindow || !m_frameContext) return;

		CommandList& cmd = m_frameContext->List;

		m_frameGraph.Execute(cmd);

		if (m_overlay) m_overlay->OnOverlayRender(cmd);

		cmd.TransitionBarrier(m_renderWindow->CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		cmd.End();

		m_graphicsQueue.Submit(cmd);
		m_renderWindow->Present();

		m_frameSlot = (m_frameSlot + 1) % FRAMES_IN_FLIGHT;
	}

	CommandList* Renderer::GetCommandList() noexcept
	{
		return m_frameContext ? &m_frameContext->List : nullptr;
	}

	void Renderer::OnDeviceLost()
	{
		KOI_LOG_CRITICAL("Graphics device lost");
	}

	void Renderer::OnDeviceRestored()
	{
		KOI_LOG_INFO("Graphics device restored");
	}

	void Renderer::Reload()
	{
		m_device->WaitForIdle();
		m_defaultResources.RebuildPipelines(*m_device);
	}
}
