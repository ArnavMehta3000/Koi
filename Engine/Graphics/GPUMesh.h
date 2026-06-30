#pragma once
#include "Graphics/Types.h"
#include <vector>

namespace koi
{
	class GfxDevice;
	struct ModelAsset;

	struct GPUMeshDraw
	{
		u32 IndexCount;
		u32 IndexOffset;
		i32 VertexOffset;

		DescriptorIndex BaseColorSRV;
		DescriptorIndex MetallicRoughnessSRV;
		DescriptorIndex NormalSRV;
		DescriptorIndex OcclusionSRV;
		DescriptorIndex EmissiveSRV;
	};

	// Owns the GPU buffers and textures for a loaded model.
	struct GPUMesh
	{
		GPUMesh() = default;
		GPUMesh(const GPUMesh&)            = delete;
		GPUMesh& operator=(const GPUMesh&) = delete;
		GPUMesh(GPUMesh&&) noexcept            = default;
		GPUMesh& operator=(GPUMesh&&) noexcept = default;

		BufferHandle VertexBuffer;
		BufferHandle IndexBuffer;
		DescriptorIndex VertexBufferSRV = INVALID_DESCRIPTOR;
		std::vector<TextureHandle> Textures;   // per-image GPU textures (not fallback)
		std::vector<GPUMeshDraw>  DrawItems;

		// Synchronous upload
		[[nodiscard]] static GPUMesh Upload(GfxDevice& device, const ModelAsset& model, DescriptorIndex fallbackAlbedoSRV);

		void Release(GfxDevice& device);
	};
}
