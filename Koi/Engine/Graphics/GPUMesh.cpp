#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <cassert>

namespace koi
{
	GPUMesh GPUMesh::Upload(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

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

	namespace
	{
		DescriptorIndex FallbackAlbedoSRV()
		{
			const DescriptorIndex srv = Engine::Get().GetSubsystem<Renderer>().GetDefaultResources().FallbackTextureSRV;
			assert(srv != INVALID_DESCRIPTOR && "Fallback texture is missing");
			return srv;
		}

	}

	// Every slot the sidecar bound resolves to a material asset, which owns its own table row and
	// its own texture references. Nothing here decodes an image: residency is the material's job
	void GPUMesh::BindFromAssets(GfxDevice& device, const ModelAsset& model)
	{
		AssetCache<Material>& cache = Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>();

		for (const AssetKey& key : model.MaterialBindings)
		{
			const MaterialHandle handle = key.IsValid() ? cache.Acquire(device, key) : MaterialHandle{};
			const Material* material = cache.Get(handle);

			Materials.push_back(handle);
			TableSlots.push_back(material ? material->TableSlot : MaterialTable::kFallbackSlot);
			SlotPipelines.push_back(material ? material->Pipeline : PipelineHandle());
		}
	}

	void GPUMesh::BindFromModel(GfxDevice& device, const ModelAsset& model)
	{
		const DescriptorIndex fallbackAlbedoSRV = FallbackAlbedoSRV();

		// Resolve every image this model references to a shared texture asset and collect their
		// SRV indices. Images that came from a standalone file are keyed by that file's path, so a
		// texture used by two different models is uploaded once
		std::vector<DescriptorIndex> imageSRVs;
		imageSRVs.reserve(model.Images.size());

		AssetCache<GPUTexture>& textures = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>();

		const AssetIdentity& identity = AssetIdentity::Get();
		std::vector<AssetKey> imageKeys;
		std::vector<AssetKey> decodeKeys;  // standalone images only; the rest carry their pixels inline
		imageKeys.reserve(model.Images.size());
		decodeKeys.reserve(model.Images.size());

		for (const ImageAsset& img : model.Images)
		{
			imageKeys.push_back(identity.KeyForPath(img.Path));
			decodeKeys.push_back(img.Pixels.empty() ? imageKeys.back() : AssetKey());
		}

		// Decode every standalone image up front, in parallel. stb_image is the bulk of scene load
		// and it doesn't touch the device, so the GPU-side work below stays single-threaded
		const std::vector<std::unique_ptr<ImageAsset>> decoded = textures.DecodeParallel(decodeKeys);

		GfxDevice::UploadContext& upload = device.GetUploadContext();
		upload.Begin();  // outer batch: each texture's own Begin/End nests into this one

		for (size_t i = 0; i < model.Images.size(); ++i)
		{
			const ImageAsset& img = model.Images[i];

			// Pixels are carried inline only for images embedded in the glTF. Everything else was
			// prefetched above; a null slot means resident, duplicated, or failed, and Acquire resolves all three
			const GPUTextureHandle handle =
				  !img.Pixels.empty() ? textures.AcquireFrom(device, imageKeys[i], img)
				: decoded[i]          ? textures.AcquireFrom(device, imageKeys[i], *decoded[i])
				:                       textures.Acquire(device, imageKeys[i]);

			const GPUTexture* texture = textures.Get(handle);
			imageSRVs.push_back(texture ? texture->SRVIndex : INVALID_DESCRIPTOR);

			if (texture)
				Textures.push_back(handle);
		}

		upload.End();

		// --- Build one GPUMaterialData per model material (shared across however many submeshes use it) ---
		auto lookupSRV = [&](const ImportMaterial& mat, DescriptorIndex ImportMaterial::* textureIdxMember) -> DescriptorIndex
		{
			// .* dereferences pointer to member, giving the value of the member for this material
			const u32 imgIdx = mat.*textureIdxMember;
			return imgIdx < imageSRVs.size() ? imageSRVs[imgIdx] : INVALID_DESCRIPTOR;
		};

		// No sidecar means no material asset owns these rows, so the mesh takes them itself
		MaterialTable& table = Engine::Get().GetSubsystem<Renderer>().GetMaterialTable();

		for (const ImportMaterial& mat : model.Materials)
		{
			const DescriptorIndex albedoSRV = lookupSRV(mat, &ImportMaterial::BaseColorTextureIdx);
			const u32 slot = table.Allocate(GPUMaterialData
			{
				.BaseColorFactor        = mat.MaterialConstants.BaseColorFactor,
				.EmissiveFactor         = Vector4(mat.MaterialConstants.EmissiveFactor.x, mat.MaterialConstants.EmissiveFactor.y, mat.MaterialConstants.EmissiveFactor.z, 0.0f),
				.MetallicFactor         = mat.MaterialConstants.MetallicFactor,
				.RoughnessFactor        = mat.MaterialConstants.RoughnessFactor,
				.NormalScale            = mat.MaterialConstants.NormalScale,
				.OcclusionStrength      = mat.MaterialConstants.OcclusionStrength,
				.AlphaCutoff            = mat.MaterialConstants.AlphaCutoff,
				.BaseColorIndex         = albedoSRV != INVALID_DESCRIPTOR ? albedoSRV : fallbackAlbedoSRV,
				.MetallicRoughnessIndex = lookupSRV(mat, &ImportMaterial::MetallicRoughnessTextureIdx),
				.NormalIndex            = lookupSRV(mat, &ImportMaterial::NormalTextureIdx),
				.OcclusionIndex         = lookupSRV(mat, &ImportMaterial::OcclusionTextureIdx),
				.EmissiveIndex          = lookupSRV(mat, &ImportMaterial::EmissiveTextureIdx),
			});

			// No material asset means no template, so the pass picks the pipeline
			OwnedSlots.push_back(slot);
			TableSlots.push_back(slot);
			SlotPipelines.push_back(PipelineHandle());
		}
	}

	void GPUMesh::BindMaterials(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		// A mesh with no sidecar still has to draw, so the glTF's own materials stand in
		if (model.MaterialBindings.empty()) BindFromModel (device, model);
		else                                BindFromAssets(device, model);

		// The slot Upload gave every submesh whose material index was missing or out of range
		TableSlots.push_back(MaterialTable::kFallbackSlot);
		SlotPipelines.push_back(PipelineHandle());

		KOI_LOG_DEBUG("GPUMesh materials bound: {} slot(s), {} material asset(s), {} texture(s)",
		              TableSlots.size(), Materials.size(), Textures.size());
	}

	void GPUMesh::Release(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		device.Destroy(VertexBuffer);
		device.Destroy(IndexBuffer);

		// Only rows this mesh allocated for itself; a material asset frees its own
		MaterialTable& table = Engine::Get().GetSubsystem<Renderer>().GetMaterialTable();
		for (const u32 slot : OwnedSlots)
		{
			table.Free(slot);
		}

		AssetRegistry& assets = Engine::Get().GetSubsystem<AssetRegistry>();

		// Each texture is destroyed only once every mesh using it has released it
		AssetCache<GPUTexture>& textures = assets.Cache<GPUTexture>();
		for (GPUTextureHandle handle : Textures)
		{
			textures.Release(device, handle);
		}

		AssetCache<Material>& materials = assets.Cache<Material>();
		for (MaterialHandle handle : Materials)
		{
			materials.Release(device, handle);
		}

		TableSlots.clear();
		SlotPipelines.clear();
		OwnedSlots.clear();
		Materials.clear();
		Textures.clear();
		DrawItems.clear();
		VertexBufferSRV = INVALID_DESCRIPTOR;
	}
}
