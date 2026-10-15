#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/MeshGather.h"

namespace koi
{
	class  World;
	class  GfxDevice;
	struct RendererResources;

	class ShadowPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device     = nullptr;
			const RendererResources* Resources  = nullptr;
			u32                      Resolution = 2048;
		};

		explicit ShadowPass(const Config& config);

		std::string_view GetName() const override { return "Shadow"; }

		// Publishes ShadowData
		void Record(RenderGraph& graph, World& world);

		[[nodiscard]] inline bool HasShadow() const { return m_hasShadow; }
		[[nodiscard]] inline const Matrix& GetLightViewProjection() const { return m_lightViewProjection; }

	private:
		// Ortho light-space view-projection fitted to the world bounds
		bool ComputeLightMatrix(World& world);
		void Draw(RasterContext& ctx, World& world);

	private:
		Config m_config;

		Matrix     m_lightViewProjection = Matrix::Identity;
		CullVolume m_cullVolume;                 // the ortho volume, as an oriented box
		bool       m_hasShadow           = false;

		GpuFrameBuffer<FrameConstants> m_frameConstants;
		GpuFrameBuffer<PerObjectData>  m_objects;
		MeshGatherer                   m_gatherer;
		MeshGatherResult               m_gather;
	};
}
