#include "Graphics/Renderer.h"
#include "Graphics/RendererResources.h"
#include "Graphics/RHI/Device.h"
#include "Graphics/RHI/Resources.h"
#include "Graphics/ShaderLoader.h"
#include "Runtime/Engine.h"
#include "Memory/MemoryTracker.h"
#include "Core/Profiling.h"
#include "Core/Logger.h"
#include <DirectXColors.h>
#include <cassert>

namespace koi
{
	Renderer::Renderer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		m_device = std::make_unique<GfxDevice>(
		GfxDevice::Desc
		{
			.EnableDebugLayer = KOI_DEBUG,
			.EnableGPUValidation = KOI_DEBUG
		});

		m_device->SetDeviceNotify(this);
		m_graphicsQueue = CommandQueue(m_device->GetGraphicsQueue());

		// Tone-mapping pipeline: fullscreen triangle VS + ACES PS, no depth, writes to swapchain format
		auto vsData = LoadShader("engine://Shaders/TonemapVS");
		auto psData = LoadShader("engine://Shaders/TonemapPS");
		assert((!vsData.empty() && !psData.empty()) && "Failed to load tonemap shaders");

		m_tonemapPipeline = m_device->CreatePipeline(PipelineDesc
		{
			.VertexShader        = vsData,
			.PixelShader         = psData,
			.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
			.DepthStencil        = { .DepthEnable = false, .DepthWrite = false },
			.RenderTargetFormats = { Format::R8G8B8A8_UNORM },  // This will render onto the swpchain
			.RenderTargetCount   = 1,
			.DebugName           = "Tonemap",
		});

		m_defaultResources.Init(*m_device);
		KOI_LOG_INFO("Initializing renderer");

#if KOI_WITH_EDITOR
		// Rebuild the tonemap pipeline when the editor invalidates the shader cache
		Engine::Get().RegisterHook(EnginePhase::OnShaderReload, [this]
		{
			m_device->WaitForIdle();
			if (m_tonemapPipeline) m_device->Destroy(m_tonemapPipeline);
			auto vsData = LoadShader("engine://Shaders/TonemapVS");
			auto psData = LoadShader("engine://Shaders/TonemapPS");
			if (vsData.empty() || psData.empty()) return;
			m_tonemapPipeline = m_device->CreatePipeline(PipelineDesc
			{
				.VertexShader        = vsData,
				.PixelShader         = psData,
				.Rasterizer          = { .CullMode = D3D12_CULL_MODE_NONE },
				.DepthStencil        = { .DepthEnable = false, .DepthWrite = false },
				.RenderTargetFormats = { Format::R8G8B8A8_UNORM },
				.RenderTargetCount   = 1,
				.DebugName           = "Tonemap",
			});
		});
#endif
	}

	Renderer::~Renderer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		// Destroy device resources
		if (m_device)
		{
			m_device->Destroy(m_tonemapPipeline);
			m_device->WaitForIdle();
		}

		m_defaultResources.Shutdown(*m_device);
		m_frameContexts.clear();
		m_renderWindows.Clear();
		m_windowMap.clear();
		m_device = nullptr;

		KOI_LOG_INFO("Shutdown renderer");
	}

	std::pair<RenderWindowHandle, RenderWindow*> Renderer::AddWindow(WindowHandle handle, bool vsync)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (auto it = m_windowMap.find(handle); it != m_windowMap.end())
			return { it->second, GetRenderWindow(it->second) };

		Window* w = Engine::Get().GetWindowManager().GetWindow(handle);

		assert(w && "Trying to create RenderWindow from invalid window handle");
		if (!w) { return { RenderWindowHandle{}, nullptr }; }

		RenderWindowHandle h = m_renderWindows.Emplace(*m_device, *w, vsync);
		m_windowMap.emplace(handle, h);

		KOI_LOG_TRACE("Created RenderWindow (Handle: idx={}, gen={})", h.GetIndex(), h.GetGeneration());
		return { h, GetRenderWindow(h) };
	}

	RenderWindowHandle Renderer::FindWindow(WindowHandle w) const noexcept
	{
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		auto it = m_windowMap.find(w);
		return it != m_windowMap.end() ? it->second : RenderWindowHandle{};
	}

	void Renderer::RemoveWindow(WindowHandle handle)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		auto it = m_windowMap.find(handle);
		if (it == m_windowMap.end()) return;

		// RenderWindow's destructor calls WaitForIdle, so it's safe to erase here.
		KOI_LOG_TRACE("Destroying RenderWindow (Handle: idx={}, gen={})", handle.GetIndex(), handle.GetGeneration());
		m_renderWindows.Erase(it->second);
		m_windowMap.erase(it);
	}

	void Renderer::BeginFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		// Submit any queued uploads on the copy queue and insert a GPU-side Wait on the
		// graphics queue, so all uploaded resources are ready before any draw work begins.
		m_device->GetUploadContext().FlushQueued(m_device->GetGraphicsQueue());

		// Open a command list for each RenderWindow and record a clear.
		m_renderWindows.ForEach([this](RenderWindowHandle h, RenderWindow& rw)
		{
			ZoneScoped;
			FrameContext& ctx = GetOrCreateFrameContext(h);

			ctx.List.Begin(m_frameSlot);

			// Clear HDR color buffer
			constexpr f32 kClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			ctx.List.TransitionBarrier(rw.GetHDRColorBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);
			ctx.List.ClearRenderTarget(rw.GetHDRColorBuffer(), kClearColor);
		});
	}

	void Renderer::EndFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_renderWindows.ForEach([this](RenderWindowHandle h, RenderWindow& rw)
		{
			ZoneScoped;
			FrameContext& ctx = GetOrCreateFrameContext(h);

			// Tone-mapping pass: read from HDR buffer, write to swapchain.
			ctx.List.TransitionBarrier(rw.GetHDRColorBuffer(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);  // HDR buffer is now readable by shader
			ctx.List.TransitionBarrier(rw.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET);          // Swapchain is now writable

			ctx.List.SetRenderTarget(rw.CurrentBackBuffer());
			ctx.List.SetViewportAndScissor(rw.GetWidth(), rw.GetHeight());
			ctx.List.SetPipeline(m_tonemapPipeline);

			const Texture* hdrTex = m_device->Get(rw.GetHDRColorBuffer());
			u32 hdrSrvIndex = hdrTex->SRVIndex;
			ctx.List.SetPushConstants(&hdrSrvIndex, 1);
			ctx.List.Draw(3);  // Draw fullscreen triangle (TonemapVS)

			// Overlay phase: editor / debug UI draws to the swapchain on top of the tonemapped image
			Engine::Get().FireHook(EnginePhase::OnSwapchainOverlay);

			ctx.List.TransitionBarrier(rw.CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);  // Swapchain is now presentable
			ctx.List.End();

			m_graphicsQueue.Submit(ctx.List);
			rw.Present();
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
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

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
