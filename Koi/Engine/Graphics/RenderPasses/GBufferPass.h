#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/MeshGather.h"

namespace koi
{
	class  World;
	class  GfxDevice;
	struct Camera;
	struct RendererResources;
	class  MaterialTable;

	class GBufferPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device          = nullptr;
			const RendererResources* Resources       = nullptr;
			MaterialTable*           Materials       = nullptr;
			const RasterPipeline*    DefaultPipeline = nullptr;  // for draws whose material names none
		};

		explicit GBufferPass(const Config& config);

		std::string_view GetName() const override { return "GBuffer"; }

		// Publishes GBufferData and SceneDepth
		void Record(RenderGraph& graph, World& world, const Camera& camera, u32 width, u32 height);

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
