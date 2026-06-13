#include "Graphics/Renderer.h"
#include "Graphics/RendererResources.h"
#include "Graphics/RenderPasses/TonemapPass.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Platform/Window.h"
#include "Runtime/Engine.h"
#include "Memory/MemoryTracker.h"
#include "Core/Profiling.h"
#include "Core/Logger.h"
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

#if KOI_WITH_EDITOR
		Engine::Get().RegisterHook(EnginePhase::OnShaderReload, [this]
		{
			m_device->WaitForIdle();
			m_defaultResources.RebuildPipelines(*m_device);
		});
#endif
	}

	Renderer::~Renderer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (m_device)
			m_device->WaitForIdle();

		m_frameContext.reset();
		m_renderWindow.reset();
		m_defaultResources.Shutdown(*m_device);
		m_device = nullptr;

		KOI_LOG_INFO("Shutdown renderer");
	}

	void Renderer::InitMainWindow(Window& window, bool vsync)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_renderWindow.emplace(*m_device, window, vsync);

		window.SetResizeCallback([this](u32 w, u32 h)
		{
			if (m_renderWindow) m_renderWindow->Resize(w, h);
		});

		m_frameContext = std::make_unique<FrameContext>();
		m_frameContext->Allocator.Init(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer Allocator");
		m_frameContext->List.Init(*m_device, m_frameContext->Allocator, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer CmdList");

		KOI_LOG_TRACE("Renderer: main window initialized");
	}

	void Renderer::BeginFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		if (!m_renderWindow || !m_frameContext) return;

		m_device->GetUploadContext().FlushQueued(m_device->GetGraphicsQueue());

		m_frameContext->List.Begin(m_frameSlot);

		constexpr f32 kClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_frameContext->List.TransitionBarrier(m_renderWindow->GetHDRColorBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_frameContext->List.ClearRenderTarget(m_renderWindow->GetHDRColorBuffer(), kClearColor);
	}

	void Renderer::EndFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		if (!m_renderWindow || !m_frameContext) return;

		CommandList& cmd = m_frameContext->List;

		m_frameGraph.Clear();
		m_frameGraph.AddPass<TonemapPass>(TonemapPass::Desc
		{
			.Device       = m_device.get(),
			.Pipeline     = m_defaultResources.TonemapPipeline,
			.HdrInput     = m_renderWindow->GetHDRColorBuffer(),
			.TargetWindow = &*m_renderWindow,
		});
		m_frameGraph.Execute(cmd);

		Engine::Get().FireHook(EnginePhase::OnSwapchainOverlay);

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
}
