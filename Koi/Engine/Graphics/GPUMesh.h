#pragma once
#include "Koi/Engine/Assets/AssetHandles.h"
#include "Koi/Engine/RHI/Types.h"
#include "Koi/Engine/Math/MathTypes.h"
#include <vector>

namespace koi
{
	class GfxDevice;
	struct ModelAsset;
	struct RasterPipeline;

	struct GPUMeshDraw
	{
		u32 IndexCount;
		u32 IndexOffset;
		i32 VertexOffset;

		u32 MaterialSlot;  // which of the mesh's material slots this submesh draws with

		BoundingBox Bounds{ {}, {} };  // model-space, per submesh: the culling granularity
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
		DescriptorIndex VertexBufferSRV = INVALID_DESCRIPTOR;

		std::vector<u32> TableSlots;                       // GPUMeshDraw::MaterialSlot -> row in the MaterialTable
		std::vector<const RasterPipeline*> SlotPipelines;  // parallel to TableSlots; null = the pass decides
		std::vector<MaterialHandle> Materials;             // slot -> material asset, empty when the mesh has no sidecar
		std::vector<GPUMeshDraw> DrawItems;
		BoundingBox Bounds{ {}, {} };                      // model-space, carried over from the source ModelAsset

		// Geometry only: buffers, draw items and bounds. Loading a mesh touches no texture
		[[nodiscard]] static GPUMesh Upload(GfxDevice& device, const ModelAsset& model);

		// Resolves the sidecar's material assets into table rows, one per material slot
		void BindMaterials(GfxDevice& device, const ModelAsset& model);

		void Release(GfxDevice& device);
	};
}
