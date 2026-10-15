#pragma once
#include "Koi/Engine/Graphics/RenderPasses/RenderPass.h"
#include "Koi/Engine/Graphics/GpuFrameBuffer.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"

namespace koi
{
	class  World;
	class  GfxDevice;
	struct Camera;
	struct RendererResources;

	class LightingPass : public RenderPass
	{
	public:
		struct Config
		{
			GfxDevice*               Device    = nullptr;
			const RendererResources* Resources = nullptr;
		};

		explicit LightingPass(const Config& config);

		std::string_view GetName() const override { return "Lighting"; }

		// Reads GBufferData, SceneDepth and ShadowData if present; publishes SceneColor
		void Record(RenderGraph& graph, World& world, const Camera& camera);

	private:
		void GatherLights(World& world, const Matrix& lightViewProjection, DescriptorIndex shadowMapIndex);

		Config                   m_config;
		GpuFrameBuffer<GPULight> m_lightBuffer;  // StructuredBuffer<GPULight>, grown on demand
		std::vector<GPULight>    m_lights;
	};
}
