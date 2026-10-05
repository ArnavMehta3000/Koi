#pragma once
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Assets/TextureAsset.h"
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	class GfxDevice;
	class PipelineLibrary;

	struct RendererResources
	{
		// Asset cache fallback resources
		MeshHandle       FallbackMesh{};
		GPUTextureHandle FallbackTexture{};
		DescriptorIndex  FallbackTextureSRV = INVALID_DESCRIPTOR;  // magenta checker: what a missing material draws with
		GPUTextureHandle WhiteTexture{};
		DescriptorIndex  WhiteTextureSRV    = INVALID_DESCRIPTOR;  // what a material samples for an unbound texture slot

		// Default samplers (indices into the bindless sampler heap)
		DescriptorIndex LinearWrapSampler  = INVALID_DESCRIPTOR;
		DescriptorIndex LinearClampSampler = INVALID_DESCRIPTOR;
		DescriptorIndex PointClampSampler  = INVALID_DESCRIPTOR;
		DescriptorIndex ShadowSampler      = INVALID_DESCRIPTOR;  // comparison sampler for SampleCmp

		// Default pipelines
		PipelineHandle TonemapPipeline{};           // fullscreen HDR -> swapchain ACES tonemap
		PipelineHandle GBufferPipeline{};           // deferred geometry: 3 MRT (albedo/normals/material) + depth
		PipelineHandle LightingPipeline{};          // full-screen deferred lighting: samples G-buffer -> HDR
		PipelineHandle ShadowPipeline{};            // depth-only shadow map: no pixel shader, no render targets
		PipelineHandle DebugLinePipeline{};         // LINELIST overlay
		PipelineHandle GridPipeline{};              // editor ground grid: blended full-screen, depth-tested

		void Init(GfxDevice& device, PipelineLibrary& library);
		void Shutdown();
		void RegisterFallbackAssets(GfxDevice& device);

	private:
		// The library owns the recipes and rebuilds them on reload; we only cache the handles
		void RegisterPipelines(GfxDevice& device, PipelineLibrary& library);
	};
}
