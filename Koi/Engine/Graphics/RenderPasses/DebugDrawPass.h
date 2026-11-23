#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/Graphics/DebugDraw.h"
#include "Koi/Engine/RHI/GpuFrameBuffer.h"

namespace koi
{
	class  GfxDevice;
	struct RendererResources;

	class DebugDrawPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
			const DebugDrawQueue*    Queue     = nullptr;
		};

		explicit DebugDrawPass(const Config& config);

		static constexpr std::string_view kName = "Debug Draw";

		// Always adds its pass: gizmos are queued after the graph is built
		void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene);

	private:
		Config                      m_config;
		GpuFrameBuffer<DebugVertex> m_vertexBuffer;
	};
}
