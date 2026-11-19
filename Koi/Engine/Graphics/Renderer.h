#pragma once
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Graphics/MaterialTemplateLibrary.h"
#include "Koi/Engine/Graphics/PipelineCache.h"
#include "Koi/Engine/Graphics/RendererOverlay.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandAllocator.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/CommandQueue.h"
#include "Koi/Engine/Graphics/TargetCapture.h"
#include <memory>
#include <optional>

namespace koi
{
	class Window;
	class World;
	struct RenderView;

	class Renderer
		: NonCopyable
		, NonMovable
	{
	public:
		Renderer();
		~Renderer();

		// Called once by Engine after the main window is created.
		void Init(Window& window, bool vsync);

		[[nodiscard]] inline GfxDevice&               GetDevice() noexcept                 { return *m_device;              }
		[[nodiscard]] inline const GfxDevice&         GetDevice() const noexcept           { return *m_device;              }
		[[nodiscard]] inline RendererResources&       GetDefaultResources() noexcept       { return m_defaultResources;     }
		[[nodiscard]] inline const RendererResources& GetDefaultResources() const noexcept { return m_defaultResources;     }
		[[nodiscard]] inline RenderWindow*            GetMainRenderWindow() noexcept       { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] inline const RenderWindow*      GetMainRenderWindow() const noexcept { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] inline FeatureList&             GetFeatures() noexcept               { return m_features;             }
		[[nodiscard]] inline const RenderGraph&       GetRenderGraph() const noexcept      { return m_graph;                }
		[[nodiscard]] inline TargetCapture&           GetTargetCapture() noexcept          { return m_capture;              }
		[[nodiscard]] inline MaterialTable&           GetMaterialTable() noexcept          { return m_materials;            }
		[[nodiscard]] inline MaterialTemplateLibrary& GetMaterialTemplates() noexcept      { return m_materialTemplates;    }
		[[nodiscard]] inline PipelineCache&           GetPipelineCache() noexcept          { return m_pipelineCache;        }
		[[nodiscard]] inline const PipelineCache&     GetPipelineCache() const noexcept    { return m_pipelineCache;        }
		[[nodiscard]] inline DebugDrawQueue&          GetDebugDraw() noexcept              { return m_debugDraw;            }
		[[nodiscard]] inline const DebugDrawQueue&    GetDebugDraw() const noexcept        { return m_debugDraw;            }

		[[nodiscard]] CommandList* GetCommandList() noexcept;

		using PipelineBuildFunc = void (*)(Renderer&, FeatureList&);
		void SetPipeline(PipelineBuildFunc build);  // Clears the feature list and rebuilds it

		// Per-frame wiring, driven from the view the app host resolved
		void SetupFrame(const RenderView& view, World& world);

		inline void SetOverlay(IRendererOverlay* overlay) noexcept { m_overlay = overlay; }

		void RequestShaderRead(TextureHandle texture);
		void FlushShaderReadRequests(CommandList& cmd);

		void BeginFrame();
		void EndFrame();

		void ReloadShaders(bool all);

	private:
		void RegisterMaterialTemplates();

	private:
		struct FrameContext
		{
			CommandAllocator Allocator;
			CommandList      List;
		};

		std::unique_ptr<GfxDevice>       m_device;
		CommandQueue                     m_graphicsQueue;
		std::optional<RenderWindow>      m_renderWindow;
		std::unique_ptr<FrameContext>    m_frameContext;
		PipelineCache                    m_pipelineCache;
		RendererResources                m_defaultResources;
		MaterialTable                    m_materials;
		MaterialTemplateLibrary          m_materialTemplates;
		FeatureList                      m_features;
		RenderGraph                      m_graph;
		TargetCapture                    m_capture;
		IRendererOverlay*                m_overlay   = nullptr;
		DebugDrawQueue                   m_debugDraw;
		std::vector<TextureHandle>       m_shaderReadRequests;  // per-frame, cleared in BeginFrame
	};
}
