module;
#include <vector>
export module Koi.Graphics.GPUMesh;

import Koi.Graphics.Types;
import Koi.Graphics.RHI.Device;

export namespace koi
{
	struct GPUMeshDraw
	{
		u32 IndexCount;
		u32 IndexOffset;
		i32 VertexOffset;
		u32 BaseColorSRV;
	};

	export struct GPUMesh
	{
		GPUMesh() = default;
		GPUMesh(const GPUMesh&)            = delete;
		GPUMesh& operator=(const GPUMesh&) = delete;
		GPUMesh(GPUMesh&&) noexcept            = default;
		GPUMesh& operator=(GPUMesh&&) noexcept = default;

		BufferHandle            VertexBuffer;
		BufferHandle            IndexBuffer;
		DescriptorIndex         VertexBufferSRV = INVALID_DESCRIPTOR;
		std::vector<TextureHandle> Textures;
		std::vector<GPUMeshDraw>   DrawItems;

		[[nodiscard]] static GPUMesh Upload(GfxDevice& device, const ModelAsset& model, DescriptorIndex fallbackAlbedoSRV);
		void Release(GfxDevice& device);
	};
}
