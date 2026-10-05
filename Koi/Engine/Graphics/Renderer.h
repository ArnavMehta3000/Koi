#pragma once
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Graphics/MaterialTemplateLibrary.h"
#include "Koi/Engine/Graphics/PipelineLibrary.h"
#include "Koi/Engine/Graphics/RendererResources.h"
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/RenderWindow.h"
#include "Koi/Engine/Graphics/RHI/CommandAllocator.h"
#include "Koi/Engine/Graphics/RHI/CommandList.h"
#include "Koi/Engine/Graphics/RHI/CommandQueue.h"
#include "Koi/Engine/Interop/Reloadable.h"
#include "Koi/Engine/Interop/RendererOverlay.h"
#include "Koi/Engine/Runtime/Subsystems.h"
#include <memory>
#include <optional>

namespace koi
{
	class IRenderPipeline;
	class Window;
	class World;
	struct RenderView;

	class Renderer
		: NonCopyable
		, NonMovable
		, public IDeviceNotify
		, public IReloadable
	{
	public:
		Renderer();
		~Renderer();

		// Called once by Engine after the main window is created.
		void Init(const SubsystemInitContext& ctx);

		[[nodiscard]] inline GfxDevice&               GetDevice() noexcept                 { return *m_device;              }
		[[nodiscard]] inline const GfxDevice&         GetDevice() const noexcept           { return *m_device;              }
		[[nodiscard]] inline RendererResources&       GetDefaultResources() noexcept       { return m_defaultResources;     }
		[[nodiscard]] inline const RendererResources& GetDefaultResources() const noexcept { return m_defaultResources;     }
		[[nodiscard]] inline RenderWindow*            GetMainRenderWindow() noexcept       { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] inline const RenderWindow*      GetMainRenderWindow() const noexcept { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] inline PassList&                GetPasses() noexcept                 { return m_passes;               }
		[[nodiscard]] inline MaterialTable&           GetMaterialTable() noexcept          { return m_materials;            }
		[[nodiscard]] inline MaterialTemplateLibrary& GetMaterialTemplates() noexcept      { return m_materialTemplates;    }
		[[nodiscard]] inline PipelineLibrary&         GetPipelineLibrary() noexcept        { return m_pipelineLibrary;      }
		[[nodiscard]] inline const PipelineLibrary&   GetPipelineLibrary() const noexcept  { return m_pipelineLibrary;      }
		[[nodiscard]] inline DebugDrawQueue&          GetDebugDraw() noexcept              { return m_debugDraw;            }
		[[nodiscard]] inline const DebugDrawQueue&    GetDebugDraw() const noexcept        { return m_debugDraw;            }

		[[nodiscard]] CommandList* GetCommandList() noexcept;
		[[nodiscard]] inline IRenderPipeline* GetPipeline() noexcept { return m_pipeline.get(); }

		void SetPipeline(std::unique_ptr<IRenderPipeline> pipeline);  // Clears the pass list and rebuilds it

	private:
		void RegisterMaterialTemplates();

	public:
		// Per-frame wiring, driven from the view the app host resolved
		void SetupFrame(const RenderView& view, World& world);

		inline void SetOverlay(IRendererOverlay* overlay) noexcept { m_overlay = overlay; }

		void RequestShaderRead(TextureHandle texture);
		void FlushShaderReadRequests(CommandList& cmd);

		void BeginFrame();
		void EndFrame();

	private:
		void OnDeviceLost() override;
		void OnDeviceRestored() override;
		void Reload(const ReloadRequest& request) override;

		struct FrameContext
		{
			CommandAllocator Allocator;
			CommandList      List;
		};

	private:
		std::unique_ptr<GfxDevice>       m_device;
		CommandQueue                     m_graphicsQueue;
		std::optional<RenderWindow>      m_renderWindow;
		std::unique_ptr<FrameContext>    m_frameContext;
		PipelineLibrary                  m_pipelineLibrary;
		RendererResources                m_defaultResources;
		MaterialTable                    m_materials;
		MaterialTemplateLibrary          m_materialTemplates;
		PassList                         m_passes;
		std::unique_ptr<IRenderPipeline> m_pipeline;
		u32                              m_frameSlot = 0;
		IRendererOverlay*                m_overlay   = nullptr;
		DebugDrawQueue                   m_debugDraw;
		std::vector<TextureHandle>       m_shaderReadRequests;  // per-frame, cleared in BeginFrame
	};
}
