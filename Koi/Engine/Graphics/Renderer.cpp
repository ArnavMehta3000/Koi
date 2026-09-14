#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Graphics/IRenderPipeline.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/RHI/GpuProfiling.h"
#include "Koi/Engine/Graphics/RHI/Resources.h"
#include "Koi/Engine/Graphics/ShaderCache.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Platform/Window.h"
#include <cassert>

namespace koi
{
	namespace
	{
		// What a mesh draws with when its material never resolved
		static GPUMaterialData FallbackMaterialRow(DescriptorIndex albedoSRV)
		{
			return GPUMaterialData
			{
				.BaseColorFactor        = Vector4::One,
				.EmissiveFactor         = Vector4::Zero,
				.MetallicFactor         = 0.0f,
				.RoughnessFactor        = 1.0f,
				.NormalScale            = 1.0f,
				.OcclusionStrength      = 1.0f,
				.AlphaCutoff            = 0.5f,
				.BaseColorIndex         = albedoSRV,
				.MetallicRoughnessIndex = INVALID_DESCRIPTOR,
				.NormalIndex            = INVALID_DESCRIPTOR,
				.OcclusionIndex         = INVALID_DESCRIPTOR,
				.EmissiveIndex          = INVALID_DESCRIPTOR,
			};
		}
	}

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

		m_defaultResources.Init(*m_device, m_pipelineLibrary);
		RegisterMaterialTemplates();

		KOI_LOG_INFO("Renderer initialized");
	}

	void Renderer::RegisterMaterialTemplates()
	{
		using enum MaterialParamType;

		// Offsets into GPUMaterialData, which is what a material row is
		const MaterialTemplate& standard = m_materialTemplates.Register(*m_device, m_pipelineLibrary,
			PipelineRecipe::GBuffer("GBuffer",
				"engine://Content/Shaders/GBufferVS",
				"engine://Content/Shaders/GBufferPS"),
			{
				{ "base_color",         Float4,  offsetof(GPUMaterialData, BaseColorFactor)        },
				{ "emissive",           Float3,  offsetof(GPUMaterialData, EmissiveFactor)         },
				{ "metallic",           Float,   offsetof(GPUMaterialData, MetallicFactor)         },
				{ "roughness",          Float,   offsetof(GPUMaterialData, RoughnessFactor)        },
				{ "normal_scale",       Float,   offsetof(GPUMaterialData, NormalScale)            },
				{ "occlusion_strength", Float,   offsetof(GPUMaterialData, OcclusionStrength)      },
				{ "alpha_cutoff",       Float,   offsetof(GPUMaterialData, AlphaCutoff)            },
				{ "base_color",         Texture, offsetof(GPUMaterialData, BaseColorIndex)         },
				{ "metallic_roughness", Texture, offsetof(GPUMaterialData, MetallicRoughnessIndex) },
				{ "normal",             Texture, offsetof(GPUMaterialData, NormalIndex)            },
				{ "occlusion",          Texture, offsetof(GPUMaterialData, OcclusionIndex)         },
				{ "emissive",           Texture, offsetof(GPUMaterialData, EmissiveIndex)          },
			});

		m_defaultResources.GBufferPipeline = standard.SingleSided;
	}

	Renderer::~Renderer()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		if (m_device)
			m_device->WaitForIdle();

		m_pipeline.reset();  // holds raw pointers into the graph
		m_renderGraph.Clear();
		m_frameContext.reset();
		m_renderWindow.reset();
		m_materials.Shutdown();
		m_pipelineLibrary.Shutdown(*m_device);
		m_defaultResources.Shutdown();
		m_device = nullptr;

		KOI_LOG_INFO("Renderer shut down");
	}

	void Renderer::Init(const SubsystemInitContext& ctx)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		Window& window = ctx.MainWindow;
		m_renderWindow.emplace(*m_device, window, ctx.VSync);

		// Only the swapchain needs the OS event | render passes size themselves off the RenderView.
		window.SetResizeCallback([this](u32 w, u32 h)
		{
			if (m_renderWindow) m_renderWindow->QueueResize(w, h);
		});

		m_frameContext = std::make_unique<FrameContext>();
		m_frameContext->Allocator.Init(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer Allocator");
		m_frameContext->List.Init(*m_device, m_frameContext->Allocator, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer CmdList");

		// This init below cannot be part of the constructor due to subsystem init-order (asset registry is created after renderer)
		m_materials.Init(*m_device, FallbackMaterialRow(INVALID_DESCRIPTOR));
		m_defaultResources.RegisterFallbackAssets(*m_device);
		m_materials.Set(MaterialTable::kFallbackSlot, FallbackMaterialRow(m_defaultResources.FallbackTextureSRV));

		SetPipeline(CreateRenderPipeline(ctx.RenderPipeline));

		KOI_LOG_DEBUG("Renderer: main render window initialized ({}x{}, vsync {})", window.GetWidth(), window.GetHeight(), ctx.VSync);
	}

	void Renderer::BeginFrame()
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);
		if (!m_renderWindow || !m_frameContext) return;

		// Before anything records against the back buffers this frame
		m_renderWindow->ApplyPendingResize();

		// Closes off the previous frame's timestamp payload and signals the graphics queue for it
		m_device->GpuProfilerNewFrame();

		m_renderGraph.ResetAll();
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

		m_renderGraph.Execute(cmd);

		if (m_overlay)
		{
			ZoneScopedN("Overlay");
			KOI_GPU_ZONE(cmd, "Overlay");
			m_overlay->OnOverlayRender(cmd);
		}

		cmd.TransitionBarrier(m_renderWindow->CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		cmd.End();

		m_graphicsQueue.Submit(cmd);
		m_renderWindow->Present();

		// After Present, since that waits on the back buffer fence and the payload is ready by then
		m_device->GpuProfilerCollect();

		m_frameSlot = (m_frameSlot + 1) % FRAMES_IN_FLIGHT;
	}

	void Renderer::SetPipeline(std::unique_ptr<IRenderPipeline> pipeline)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Renderer);

		m_device->WaitForIdle();
		m_pipeline.reset();
		m_renderGraph.Clear();

		m_pipeline = std::move(pipeline);
		if (m_pipeline)
			m_pipeline->Build(*this, m_renderGraph);
	}

	void Renderer::SetupFrame(const RenderView& view, World& world)
	{
		ZoneScoped;
		if (m_pipeline)
			m_pipeline->Setup(view, world);
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

	void Renderer::Reload(const ReloadRequest& request)
	{
		ShaderCache& shaders = ShaderCache::Get();

		if (request.Scope == ReloadScope::All)
		{
			shaders.InvalidateAll();
			m_device->WaitForIdle();
			m_pipelineLibrary.RebuildAll(*m_device);
			return;
		}

		const std::vector<std::string> changed = shaders.InvalidateStale();
		if (changed.empty())
			return;  // nothing recompiled, so nothing to rebuild and no reason to stall

		m_device->WaitForIdle();
		m_pipelineLibrary.RebuildUsing(*m_device, changed);
	}
}
