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
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
			MaterialTable*           Materials = nullptr;
		};

		struct Output
		{
			RGTexture Albedo;    // RGB + AO (RGBA8_UNORM)
			RGTexture Normals;   // XYZ + MatID (RGBA16_UNORM)
			RGTexture Material;  // Roughness/Metallic/Specular + unused A (RGBA8_UNORM)
			RGTexture Depth;     // D32
		};

		explicit GBufferPass(const Config& config);

		std::string_view GetName() const override { return "GBuffer"; }

		Output Record(RenderGraph& graph, World& world, const Camera& camera, u32 width, u32 height);

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
