#pragma once
#include "Graphics/Format.h"
#include "Graphics/RenderWindow.h"
#include "Graphics/RendererResources.h"
#include "Graphics/RenderPasses/RenderPass.h"
#include "Graphics/RHI/CommandAllocator.h"
#include "Graphics/RHI/CommandList.h"
#include "Graphics/RHI/CommandQueue.h"
#include "Editor/RendererOverlay.h"
#include "Editor/Reloadable.h"
#include <memory>
#include <optional>

namespace koi
{
	class Window;

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
		void InitMainWindow(Window& window, bool vsync, Format depthFormat = Format::D32_FLOAT);

		[[nodiscard]] inline GfxDevice&               GetDevice() noexcept                 { return *m_device;              }
		[[nodiscard]] inline const GfxDevice&         GetDevice() const noexcept           { return *m_device;              }
		[[nodiscard]] inline RendererResources&       GetDefaultResources() noexcept       { return m_defaultResources;     }
		[[nodiscard]] inline const RendererResources& GetDefaultResources() const noexcept { return m_defaultResources;     }
		[[nodiscard]] inline RenderWindow*            GetMainRenderWindow() noexcept       { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] inline const RenderWindow*      GetMainRenderWindow() const noexcept { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] inline TextureHandle            GetDepthBuffer() const noexcept      { return m_depthTexture;         }
		[[nodiscard]] inline Format                   GetDepthFormat() const noexcept      { return m_depthFormat;          }
		[[nodiscard]] inline RenderGraph&             GetFrameGraph() noexcept             { return m_frameGraph;           }

		[[nodiscard]] CommandList* GetCommandList() noexcept;

		void SetOverlay(IRendererOverlay* overlay) noexcept { m_overlay = overlay; }

		void BeginFrame();
		void EndFrame();

	private:
		void OnDeviceLost() override;
		void OnDeviceRestored() override;
		void Reload() override;

		void RebuildDepthBuffer(u32 w, u32 h);

		struct FrameContext
		{
			CommandAllocator Allocator;
			CommandList      List;
		};

	private:
		std::unique_ptr<GfxDevice>    m_device;
		CommandQueue                  m_graphicsQueue;
		std::optional<RenderWindow>   m_renderWindow;
		std::unique_ptr<FrameContext> m_frameContext;
		RendererResources             m_defaultResources;
		RenderGraph                   m_frameGraph;
		TextureHandle                 m_depthTexture;
		Format                        m_depthFormat = Format::UNKNOWN;
		u32                           m_frameSlot   = 0;
		IRendererOverlay*             m_overlay     = nullptr;
	};
}
