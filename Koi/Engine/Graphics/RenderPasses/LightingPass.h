#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Lighting.hlsli"
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/RHI/GpuFrameBuffer.h"

namespace koi
{
	class  RenderScene;
	class  GfxDevice;
	struct RendererResources;

	class LightingPass
	{
	public:
		static constexpr std::string_view kName = "Lighting";

		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
		};

		explicit LightingPass(const Config& config);
		void Record(RenderGraph& graph, const RenderView& view, const RenderScene& scene);

	private:
		void GatherLights(const RenderScene& scene, const Matrix& lightViewProjection, DescriptorIndex shadowMapIndex);

		Config                   m_config;
		GpuFrameBuffer<GPULight> m_lightBuffer;  // StructuredBuffer<GPULight>, grown on demand
		std::vector<GPULight>    m_lights;
	};
}
