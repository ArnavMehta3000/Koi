#pragma once
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Assets/TextureAsset.h"
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	class GfxDevice;

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

		void Init(GfxDevice& device);
		void Shutdown();
		void RegisterFallbackAssets(GfxDevice& device);
	};
}
