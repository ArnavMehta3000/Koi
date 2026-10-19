#pragma once
#include "Koi/Engine/Content/Shaders/Interop/Lighting.hlsli"
#include "Koi/Engine/Graphics/RenderFeature.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"

namespace koi
{
	class  World;
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
		void Record(RenderGraph& graph, const RenderView& view, World& world);

	private:
		void GatherLights(World& world, const Matrix& lightViewProjection, DescriptorIndex shadowMapIndex);

		Config                   m_config;
		GpuFrameBuffer<GPULight> m_lightBuffer;  // StructuredBuffer<GPULight>, grown on demand
		std::vector<GPULight>    m_lights;
	};
}
