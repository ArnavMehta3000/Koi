#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Graphics/DeferredPipeline.h"
#include "Koi/Engine/Graphics/Screenshot.h"
#include "Koi/Engine/RHI/Device.h"
#include "Koi/Engine/RHI/GpuProfiling.h"
#include "Koi/Engine/RHI/ShaderCache.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Platform/Window.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <cassert>
#include <utility>

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

		const Engine::Desc& engineDesc = Engine::Get().GetDesc();
		m_device = std::make_unique<GfxDevice>(GfxDevice::Desc
		{
			.EnableDebugLayer    = engineDesc.D3DDebug,
			.EnableGPUValidation = engineDesc.GpuValidation,
		});

		m_graphicsQueue = CommandQueue(m_device->GetGraphicsQueue());

		m_pipelineCache.Init(*m_device);
		m_defaultResources.Init(*m_device);
		m_capture.Init(*m_device);
		RegisterMaterialTemplates();

		KOI_LOG(Render, Debug, "Renderer initialized");
	}

	void Renderer::RegisterMaterialTemplates()
	{
		using enum MaterialParamType;

		// Offsets into GPUMaterialData, which is what a material row is
		m_materialTemplates.Register("GBuffer",
			RasterPipeline
			{
				.VertexShader = "engine://Content/Shaders/GBufferVS",
				.PixelShader  = "engine://Content/Shaders/GBufferPS",
			},
			"engine://Content/Shaders/GBufferMaskedPS",
			{   // std::vector<MaterialParams>
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
	}

	Renderer::~Renderer()
	{
		ZoneScoped;

		if (m_device)
			m_device->WaitForIdle();

		m_mainScene.reset();
		m_capture.Shutdown();
		m_frameContext.reset();
		m_renderWindow.reset();
		m_materials.Shutdown();
		m_pipelineCache.Shutdown();
		m_defaultResources.Shutdown();
		m_device = nullptr;

		KOI_LOG(Render, Debug, "Renderer shut down");
	}

	void Renderer::Init(Window& window, bool vsync)
	{
		ZoneScoped;

		m_renderWindow.emplace(*m_device, window, vsync);

		// Only the swapchain needs the OS event | render passes size themselves off the RenderView.
		window.SetResizeCallback([this](u32 w, u32 h)
		{
			if (m_renderWindow) m_renderWindow->QueueResize(w, h);
		});
		KOI_LOG(Render, Debug, "Renderer: main render window initialized ({}x{}, vsync {})", window.GetWidth(), window.GetHeight(), vsync);

		m_frameContext = std::make_unique<FrameContext>();
		m_frameContext->Allocator.Init(*m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer Allocator");
		m_frameContext->List.Init(*m_device, m_frameContext->Allocator, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Renderer CmdList");

		// This init below cannot be part of the constructor due to subsystem init-order (asset registry is created after renderer)
		m_materials.Init(*m_device, FallbackMaterialRow(INVALID_DESCRIPTOR));
		m_defaultResources.RegisterFallbackAssets(*m_device);
		m_materials.Set(MaterialTable::kFallbackSlot, FallbackMaterialRow(m_defaultResources.FallbackTextureSRV));

		RegisterPipeline("Deferred", &DeferredPipeline::Build);
		m_mainScene = std::make_unique<SceneRenderer>(*this, "Deferred");
		m_mainScene->SetCapture(&m_capture);
	}

	void Renderer::BeginFrame()
	{
		KOI_SCOPE("Renderer Begin");
		if (!m_renderWindow || !m_frameContext) return;

		// Before anything records against the back buffers this frame
		m_renderWindow->ApplyPendingResize();

		// Closes off the previous frame's timestamp payload and signals the graphics queue for it
		m_device->GpuProfilerNewFrame();

		m_shaderReadRequests.clear();
		m_submitted.clear();

		m_frameContext->List.Begin(m_device->GetFrameSlot());
	}

	void Renderer::RequestShaderRead(TextureHandle texture)
	{
		if (texture)
			m_shaderReadRequests.push_back(texture);
	}

	void Renderer::FlushShaderReadRequests(CommandList& cmd)
	{
		ZoneScoped;
		cmd.TransitionBarriers(m_shaderReadRequests, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmd.FlushBarriers();
		m_shaderReadRequests.clear();
	}

	void Renderer::EndFrame()
	{
		ZoneScoped;
		if (!m_renderWindow || !m_frameContext) return;

		CommandList& cmd = m_frameContext->List;

		// Nothing will write the back buffer (no camera), and flip-discard leaves it undefined
		if (m_submitted.empty() && !m_overlay)
		{
			static constexpr f32 kBlack[] = { 0.0f, 0.0f, 0.0f, 1.0f };
			cmd.ClearRenderTarget(m_renderWindow->CurrentBackBuffer(), kBlack);
		}

		const bool mainSubmitted = std::ranges::contains(m_submitted, m_mainScene.get());
		for (SceneRenderer* scene : std::exchange(m_submitted, {}))
		{
			scene->Execute(cmd);
		}
		m_debugDraw.Clear();  // after the pass consumed it, so ImGui-time pushes (inspectors) survive into the next frame

		if (m_overlay)
		{
			ZoneScopedN("Overlay");
			KOI_GPU_ZONE(cmd, "Overlay");
			m_overlay->OnOverlayRender(cmd);
		}

		{
			KOI_SCOPE("Submit");
			m_device->GetGpuTimers().Resolve(cmd.GetNative());
			cmd.TransitionBarrier(m_renderWindow->CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
			cmd.End();
			m_graphicsQueue.Submit(cmd);
		}

		if (m_screenshot)
		{
			TakeScreenshot(mainSubmitted ? m_mainScene->GetOutput() : TextureHandle());
		}
		m_renderWindow->Present();

		// After Present, since that waits on the back buffer fence and the payload is ready by then
		m_device->GpuProfilerCollect();
	}

	void Renderer::RequestScreenshot(fs::path path)
	{
		if (path.empty())
		{
			path = utils::TimestampedScreenshotPath(Engine::Get().GetDesc().SavedDir / "Screenshots");
		}
		m_screenshot = std::move(path);
	}

	void Renderer::TakeScreenshot(TextureHandle target)
	{
		const fs::path path = std::move(*m_screenshot);
		m_screenshot.reset();

		if (!target)
		{
			KOI_LOG(Render, Warn, "No screenshot taken: nothing rendered to the main view this frame");
			return;
		}

		if (const Result<void> saved = utils::SaveScreenshot(*m_device, target, path); !saved)
		{
			KOI_LOG(Render, Error, "Couldn't save screenshot '{}': {}", path.string(), saved.error().Message);
			return;
		}
		KOI_LOG(Render, Info, "Saved screenshot '{}'", path.string());
	}

	void Renderer::RegisterPipeline(std::string name, PipelineBuildFunc build)
	{
		KOI_LOG(Render, Trace, "Registered render pipeline '{}'", name);
		m_pipelines.insert_or_assign(std::move(name), build);
	}

	Renderer::PipelineBuildFunc Renderer::FindPipeline(std::string_view name) const
	{
		const auto it = m_pipelines.find(std::string(name));
		return it != m_pipelines.end() ? it->second : nullptr;
	}

	void Renderer::Enqueue(SceneRenderer& scene)
	{
		if (!std::ranges::contains(m_submitted, &scene))
		{
			m_submitted.push_back(&scene);
		}
	}

	void Renderer::Dequeue(SceneRenderer& scene)
	{
		std::erase(m_submitted, &scene);
	}

	CommandList* Renderer::GetCommandList() noexcept
	{
		return m_frameContext ? &m_frameContext->List : nullptr;
	}

	void Renderer::ReloadShaders(bool all)
	{
		ShaderCache& shaders = ShaderCache::Get();

		if (all)
		{
			shaders.InvalidateAll();
			m_device->WaitForIdle();
			m_pipelineCache.RebuildAll();
			return;
		}

		const std::vector<std::string> changed = shaders.InvalidateStale();
		if (changed.empty())
			return;  // nothing recompiled, so nothing to rebuild and no reason to stall

		m_device->WaitForIdle();
		m_pipelineCache.RebuildUsing(changed);
	}
}
