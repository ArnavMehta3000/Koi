#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/MeshGather.h"

namespace koi
{
	class  World;
	class  GfxDevice;
	struct Camera;
	struct RendererResources;
	class  MaterialTable;

	class GBufferPass
	{
	public:
		static constexpr std::string_view kName = "GBuffer";

		struct Config
		{
			GfxDevice*               Device          = nullptr;
			const RendererResources* Resources       = nullptr;
			MaterialTable*           Materials       = nullptr;
			const RasterPipeline*    DefaultPipeline = nullptr;  // for draws whose material names none
		};

		explicit GBufferPass(const Config& config);
		void Record(RenderGraph& graph, const RenderView& view, World& world);

	private:
		void Draw(RasterContext& ctx, World& world, const Camera& camera);

	private:
		Config m_config;

		GpuFrameBuffer<FrameConstants> m_frameConstants;
		GpuFrameBuffer<PerObjectData>  m_objects;
		MeshGatherer                   m_gatherer;
		MeshGatherResult               m_gather;
	};
}
