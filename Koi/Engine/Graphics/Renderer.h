#pragma once
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Graphics/MaterialTemplateLibrary.h"
#include "Koi/Engine/Graphics/PipelineCache.h"
#include "Koi/Engine/Graphics/RendererOverlay.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/Graphics/SceneRenderer.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandAllocator.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/CommandQueue.h"
#include "Koi/Engine/Graphics/TargetCapture.h"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace koi
{
	class Window;

	class Renderer
		: NonCopyable
		, NonMovable
	{
	public:
		using PipelineBuildFunc = void (*)(Renderer&, FeatureList&);

		Renderer();
		~Renderer();

		void Init(Window& window, bool vsync);

		[[nodiscard]] GfxDevice&               GetDevice() noexcept                 { return *m_device;          }
		[[nodiscard]] const GfxDevice&         GetDevice() const noexcept           { return *m_device;          }
		[[nodiscard]] RendererResources&       GetDefaultResources() noexcept       { return m_defaultResources; }
		[[nodiscard]] const RendererResources& GetDefaultResources() const noexcept { return m_defaultResources; }
		[[nodiscard]] RenderWindow*            GetMainRenderWindow() noexcept       { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] const RenderWindow*      GetMainRenderWindow() const noexcept { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] TargetCapture&           GetTargetCapture() noexcept          { return m_capture;          }
		[[nodiscard]] MaterialTable&           GetMaterialTable() noexcept          { return m_materials;        }
		[[nodiscard]] MaterialTemplateLibrary& GetMaterialTemplates() noexcept      { return m_materialTemplates;}
		[[nodiscard]] PipelineCache&           GetPipelineCache() noexcept          { return m_pipelineCache;    }
		[[nodiscard]] const PipelineCache&     GetPipelineCache() const noexcept    { return m_pipelineCache;    }
		[[nodiscard]] DebugDrawQueue&          GetDebugDraw() noexcept              { return m_debugDraw;        }
		[[nodiscard]] const DebugDrawQueue&    GetDebugDraw() const noexcept        { return m_debugDraw;        }
		[[nodiscard]] SceneRenderer&           GetMainSceneRenderer() noexcept      { return *m_mainScene;       }

		[[nodiscard]] CommandList* GetCommandList() noexcept;

		void RegisterPipeline(std::string name, PipelineBuildFunc build);
		[[nodiscard]] PipelineBuildFunc FindPipeline(std::string_view name) const;

		void SetOverlay(IRendererOverlay* overlay) noexcept { m_overlay = overlay; }

		void RequestShaderRead(TextureHandle texture);
		void FlushShaderReadRequests(CommandList& cmd);

		void BeginFrame();
		void EndFrame();

		void ReloadShaders(bool all);

	private:
		friend class SceneRenderer;
		void Enqueue(SceneRenderer& scene);
		void Dequeue(SceneRenderer& scene);

		void RegisterMaterialTemplates();

	private:
		struct FrameContext
		{
			CommandAllocator Allocator;
			CommandList      List;
		};

		using PipelineMap = std::unordered_map<std::string, PipelineBuildFunc>;

		std::unique_ptr<GfxDevice>       m_device;
		CommandQueue                     m_graphicsQueue;
		std::optional<RenderWindow>      m_renderWindow;
		std::unique_ptr<FrameContext>    m_frameContext;
		PipelineCache                    m_pipelineCache;
		RendererResources                m_defaultResources;
		MaterialTable                    m_materials;
		MaterialTemplateLibrary          m_materialTemplates;
		TargetCapture                    m_capture;
		PipelineMap                      m_pipelines;
		std::unique_ptr<SceneRenderer>   m_mainScene;
		std::vector<SceneRenderer*>      m_submitted;  // this frame's views, executed in order by EndFrame
		IRendererOverlay*                m_overlay   = nullptr;
		DebugDrawQueue                   m_debugDraw;
		std::vector<TextureHandle>       m_shaderReadRequests;  // per-frame, cleared in BeginFrame
	};
}
