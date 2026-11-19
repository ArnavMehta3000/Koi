#pragma once
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/RenderScene.h"

namespace koi
{
	class  RenderScene;
	class  GfxDevice;
	struct RendererResources;

	class ShadowPass
	{
	public:
		static constexpr std::string_view kName = "Shadow";

		struct Config
		{
			GfxDevice*               Device     = nullptr;
			const RendererResources* Resources  = nullptr;
			u32                      Resolution = 2048;
		};

		explicit ShadowPass(const Config& config);
		void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene);

		[[nodiscard]] inline bool HasShadow() const { return m_hasShadow; }
		[[nodiscard]] inline const Matrix& GetLightViewProjection() const { return m_lightViewProjection; }

	private:
		// Ortho light-space view-projection fitted to the world bounds
		bool ComputeLightMatrix(const RenderScene& scene);
		void Draw(RasterContext& ctx, const RenderScene& scene);

	private:
		Config m_config;

		Matrix     m_lightViewProjection = Matrix::Identity;
		CullVolume m_cullVolume;                 // the ortho volume, as an oriented box
		bool       m_hasShadow           = false;

		GpuFrameBuffer<FrameConstants> m_frameConstants;
		GpuFrameBuffer<PerObjectData>  m_objects;
		MeshGatherResult               m_gather;
	};
}
