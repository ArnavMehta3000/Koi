#pragma once
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	class GfxDevice;
	class PipelineLibrary;

	struct RendererResources
	{
		// Default textures
		TextureHandle   WhiteTexture{};       // 1x1 opaque white (0xFFFFFFFF)
		TextureHandle   FlatNormalTexture{};  // 1x1 flat normal (128, 128, 255, 255)

		// Default samplers (indices into the bindless sampler heap)
		DescriptorIndex LinearWrapSampler  = INVALID_DESCRIPTOR;
		DescriptorIndex LinearClampSampler = INVALID_DESCRIPTOR;
		DescriptorIndex PointClampSampler  = INVALID_DESCRIPTOR;

		// Default pipelines
		PipelineHandle TonemapPipeline{};           // fullscreen HDR -> swapchain ACES tonemap
		PipelineHandle MeshOpaquePipeline{};        // solid fill, back-face cull, depth read/write, HDR RT
		PipelineHandle MeshWireframePipeline{};     // wireframe fill, no cull, depth read/write, HDR RT
		PipelineHandle GBufferPipeline{};           // deferred geometry: 3 MRT (albedo/normals/material) + depth
		PipelineHandle LightingPipeline{};          // full-screen deferred lighting: samples G-buffer -> HDR

		void Init(GfxDevice& device, PipelineLibrary& library);
		void Shutdown(GfxDevice& device);

	private:
		// The library owns the recipes and rebuilds them on reload; we only keep the handles
		void RegisterPipelines(GfxDevice& device, PipelineLibrary& library);
	};
}
