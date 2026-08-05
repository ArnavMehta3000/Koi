#pragma once
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/GPUTexture.h"
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

		u32 MaterialIndex; // element index into the owning GPUMesh's MaterialBuffer
	};

	struct GPUMesh
	{
		GPUMesh() = default;
		GPUMesh(const GPUMesh&)            = delete;
		GPUMesh& operator=(const GPUMesh&) = delete;
		GPUMesh(GPUMesh&&) noexcept            = default;
		GPUMesh& operator=(GPUMesh&&) noexcept = default;

		BufferHandle VertexBuffer;
		BufferHandle IndexBuffer;
		BufferHandle MaterialBuffer;
		DescriptorIndex VertexBufferSRV   = INVALID_DESCRIPTOR;
		DescriptorIndex MaterialBufferSRV = INVALID_DESCRIPTOR;
		std::vector<GPUTextureHandle> Textures;  // referenced, not owned: the texture AssetCache owns them
		std::vector<GPUMeshDraw> DrawItems;
		AABB Bounds;                           // model-space, carried over from the source ModelAsset

		// Synchronous upload
		[[nodiscard]] static GPUMesh Upload(GfxDevice& device, const ModelAsset& model);

		void Release(GfxDevice& device);
	};
}
