#pragma once
#include "Koi/Engine/Assets/MaterialAsset.h"
#include "Koi/Engine/Graphics/Types.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
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

		std::vector<u32>              TableSlots;  // GPUMeshDraw::MaterialSlot -> row in the MaterialTable
		std::vector<PipelineHandle>   SlotPipelines;  // parallel to TableSlots; invalid = the pass decides
		std::vector<MaterialHandle>   Materials;   // slot -> material asset, empty when the mesh has no sidecar
		std::vector<u32>              OwnedSlots;  // rows this mesh allocated itself, from a sidecar-less glTF
		std::vector<GPUTextureHandle> Textures;    // referenced, not owned: the texture AssetCache owns them
		std::vector<GPUMeshDraw> DrawItems;
		BoundingBox Bounds{ {}, {} };          // model-space, carried over from the source ModelAsset

		// Geometry only: buffers, draw items and bounds. Loading a mesh touches no texture
		[[nodiscard]] static GPUMesh Upload(GfxDevice& device, const ModelAsset& model);

		// Acquires the textures the model names and fills the material buffer its slots index
		void BindMaterials(GfxDevice& device, const ModelAsset& model);

		void Release(GfxDevice& device);

	private:
		// One table slot per material slot, from the sidecar's material assets or, for a mesh
		// that was never imported, rows this mesh allocates and owns itself
		void BindFromAssets(GfxDevice& device, const ModelAsset& model);
		void BindFromModel (GfxDevice& device, const ModelAsset& model);
	};
}
