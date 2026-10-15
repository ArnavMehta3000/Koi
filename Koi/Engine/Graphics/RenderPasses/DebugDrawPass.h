#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"

namespace koi
{
	class  GfxDevice;
	struct Camera;
	struct RendererResources;

	class DebugDrawPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
			const DebugDrawQueue*    Queue     = nullptr;
		};

		explicit DebugDrawPass(const Config& config);

		inline std::string_view GetName() const override { return "Debug Draw"; }

		// Always adds its pass: gizmos are queued after the graph is built
		void Record(RenderGraph& graph, const Camera& camera);

	private:
		Config                      m_config;
		GpuFrameBuffer<DebugVertex> m_vertexBuffer;
	};
}
