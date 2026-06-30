module;
#include <memory>
#include <optional>
export module Koi.Graphics.Renderer;

import Koi.Core.Types;
import Koi.Graphics.Format;
import Koi.Graphics.Types;
import Koi.Graphics.RenderWindow;
import Koi.Graphics.RendererResources;
import Koi.Graphics.RenderPass;
import Koi.Graphics.RHI.CommandAllocator;
import Koi.Graphics.RHI.CommandList;
import Koi.Graphics.RHI.CommandQueue;
import Koi.Graphics.RHI.Device;

export namespace koi
{
	class Window;

	class Renderer : NonCopyable, NonMovable, public IDeviceNotify
	{
	public:
		Renderer();
		~Renderer();

		void InitMainWindow(Window& window, bool vsync, Format depthFormat = Format::D32_FLOAT);

		[[nodiscard]] GfxDevice&               GetDevice()            noexcept       { return *m_device;          }
		[[nodiscard]] const GfxDevice&         GetDevice()            const noexcept { return *m_device;          }
		[[nodiscard]] RendererResources&       GetDefaultResources()  noexcept       { return m_defaultResources; }
		[[nodiscard]] const RendererResources& GetDefaultResources()  const noexcept { return m_defaultResources; }
		[[nodiscard]] RenderWindow*            GetMainRenderWindow()  noexcept       { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] const RenderWindow*      GetMainRenderWindow()  const noexcept { return m_renderWindow ? &*m_renderWindow : nullptr; }
		[[nodiscard]] TextureHandle            GetDepthBuffer()       const noexcept { return m_depthTexture;     }
		[[nodiscard]] RenderGraph&             GetFrameGraph()        noexcept       { return m_frameGraph;       }
		[[nodiscard]] CommandList*             GetCommandList()       noexcept;

		void BeginFrame();
		void EndFrame();

	private:
		void OnDeviceLost()     override;
		void OnDeviceRestored() override;
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
	};
}
