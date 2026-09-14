#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <cassert>

namespace koi
{
	GPUMesh GPUMesh::Upload(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;

		GPUMesh mesh;
		mesh.Bounds = model.Bounds;

		// Submeshes with no material land on a slot one past the last one the model declares
		const u32 defaultSlot = static_cast<u32>(model.Materials.size());

		// Flatten all mesh vertices and indices into a single pair of buffers.
		std::vector<Vertex> vertices;
		std::vector<u32>    indices;

		for (const Mesh& m : model.Meshes)
		{
			const u32 baseVertex = static_cast<u32>(vertices.size());
			const u32 baseIndex  = static_cast<u32>(indices.size());

			vertices.insert(vertices.end(), m.Vertices.begin(), m.Vertices.end());
			indices.insert(indices.end(),   m.Indices.begin(),  m.Indices.end());

			// Get sub mesh data
			for (const SubMesh& sub : m.SubMeshes)
			{
				const bool validMaterial = sub.MaterialIndex >= 0
					&& static_cast<size_t>(sub.MaterialIndex) < model.Materials.size();

				mesh.DrawItems.push_back(GPUMeshDraw
				{
					.IndexCount   = sub.IndexCount,
					.IndexOffset  = baseIndex  + sub.IndexOffset,
					.VertexOffset = static_cast<i32>(baseVertex + sub.VertexOffset),
					.MaterialSlot = validMaterial ? static_cast<u32>(sub.MaterialIndex) : defaultSlot,
					.Bounds       = sub.Bounds,
				});
			}
		}

		const u64 vbSize = vertices.size() * sizeof(Vertex);
		const u64 ibSize = indices.size()  * sizeof(u32);
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

		KOI_LOG_DEBUG("GPUMesh geometry uploaded: {} draws, {} verts, {} indices",
		              mesh.DrawItems.size(), vertices.size(), indices.size());

		return mesh;
	}

	// Every slot the sidecar bound resolves to a material asset, which owns its own table row and
	// its own texture references. A mesh that was never imported draws every slot with the fallback
	void GPUMesh::BindMaterials(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;
		AssetCache<Material>& cache = Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>();

		for (size_t i = 0; i < model.Materials.size(); ++i)
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

		if (model.MaterialBindings.empty() && !model.Materials.empty())
		{
			KOI_LOG_WARN("Mesh '{}' has no .mesh.toml sidecar; import it to bind materials", model.SourcePath);
		}

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
