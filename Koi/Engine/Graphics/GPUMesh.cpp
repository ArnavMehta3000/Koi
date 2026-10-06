#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Assets/AssetData.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/MeshFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/MaterialTable.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <cassert>

namespace koi
{
	namespace
	{
		BoundingBox BoxFrom(const f32 (&min)[3], const f32 (&max)[3])
		{
			const Vector3 lo(min), hi(max);
			return BoundingBox((lo + hi) * 0.5f, (hi - lo) * 0.5f);
		}

		void CreateBuffers(GfxDevice& device, GPUMesh& mesh, std::span<const Vertex> vertices, std::span<const u32> indices)
		{
			const u64 vbSize = vertices.size_bytes();
			const u64 ibSize = indices.size_bytes();
			assert(vbSize > 0 && ibSize > 0 && "GPUMesh::Upload: model has no geometry");

			mesh.VertexBuffer = device.CreateBuffer(BufferDesc
			{
				.Size      = vbSize,
				.Stride    = sizeof(Vertex),
				.Usage     = BufferUsage::Storage | BufferUsage::CopyDest,
				.Memory    = MemoryUsage::GpuOnly,
				.DebugName = "Mesh VB",
			});

			mesh.IndexBuffer = device.CreateBuffer(BufferDesc
			{
				.Size      = ibSize,
				.Stride    = sizeof(u32),
				.Usage     = BufferUsage::Index | BufferUsage::CopyDest,
				.Memory    = MemoryUsage::GpuOnly,
				.DebugName = "Mesh IB",
			});

			GfxDevice::UploadContext& upload = device.GetUploadContext();
			upload.Begin();
			upload.UploadBuffer(mesh.VertexBuffer, vertices.data(), vbSize);
			upload.UploadBuffer(mesh.IndexBuffer,  indices.data(),  ibSize);
			upload.End();

			mesh.VertexBufferSRV = device.Get(mesh.VertexBuffer)->SRVIndex;
			assert(mesh.VertexBufferSRV != INVALID_DESCRIPTOR && "Vertex buffer has no bindless SRV");
		}

		// A .kmesh is already one flat vertex and index buffer, so it goes up without any copying
		GPUMesh UploadPrepared(GfxDevice& device, const ModelAsset& model)
		{
			const PreparedMesh& prepared = *model.Prepared;

			GPUMesh mesh;
			mesh.Bounds = model.Bounds;
			mesh.DrawItems.reserve(prepared.Submeshes.size());
			for (const MeshSubmesh& sub : prepared.Submeshes)
			{
				mesh.DrawItems.push_back(GPUMeshDraw
				{
					.IndexCount   = sub.IndexCount,
					.IndexOffset  = sub.IndexOffset,
					.VertexOffset = sub.VertexOffset,
					.MaterialSlot = sub.MaterialSlot,
					.Bounds       = BoxFrom(sub.BoundsMin, sub.BoundsMax),
				});
			}

			CreateBuffers(device, mesh, prepared.Vertices, prepared.Indices);
			KOI_LOG_DEBUG("GPUMesh uploaded from {}: {} draws, {} verts, {} indices",
				model.SourcePath, mesh.DrawItems.size(), prepared.Vertices.size(), prepared.Indices.size());
			return mesh;
		}
	}

	GPUMesh GPUMesh::Upload(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;

		assert(model.Prepared && "GPUMesh::Upload: meshes only come from .kmesh files");
		return UploadPrepared(device, model);
	}

	// Every slot resolves to a material asset, which owns its own table row and its own texture
	// references. A slot with no material draws with the fallback
	void GPUMesh::BindMaterials(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;
		AssetCache<Material>& cache = Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>();

		const size_t slotCount = model.Prepared->Slots.size();
		for (size_t i = 0; i < slotCount; ++i)
		{
			const AssetKey* key = i < model.MaterialBindings.size() ? &model.MaterialBindings[i] : nullptr;

			const MaterialHandle handle = key && key->IsValid() ? cache.Acquire(device, *key) : MaterialHandle{};
			const Material* material = cache.Get(handle);

			Materials.push_back(handle);
			TableSlots.push_back(material ? material->TableSlot : MaterialTable::kFallbackSlot);
			SlotPipelines.push_back(material ? material->Pipeline : PipelineHandle());
		}

		// The slot Upload gave every submesh whose material index was missing or out of range
		TableSlots.push_back(MaterialTable::kFallbackSlot);
		SlotPipelines.push_back(PipelineHandle());

		KOI_LOG_DEBUG("GPUMesh materials bound: {} slot(s), {} material asset(s)", TableSlots.size(), Materials.size());
	}

	void GPUMesh::Release(GfxDevice& device)
	{
		device.Destroy(VertexBuffer);
		device.Destroy(IndexBuffer);

		AssetCache<Material>& materials = Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>();
		for (MaterialHandle handle : Materials)
		{
			materials.Release(device, handle);
		}

		TableSlots.clear();
		SlotPipelines.clear();
		Materials.clear();
		DrawItems.clear();
		VertexBufferSRV = INVALID_DESCRIPTOR;
	}
}
