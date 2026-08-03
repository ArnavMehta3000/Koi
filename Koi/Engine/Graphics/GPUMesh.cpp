#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/GPUSceneTypes.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"
#include <cassert>

namespace koi
{
	GPUMesh GPUMesh::Upload(GfxDevice& device, const ModelAsset& model)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		const DescriptorIndex fallbackAlbedoSRV = [&device]()
		{
			auto handle = Engine::Get().GetSubsystem<Renderer>().GetDefaultResources().WhiteTexture;
			Texture* tex = device.Get(handle);
			assert(tex && tex->Resource && "Default white texture is missing");
			return tex->SRVIndex;
		}();

		GPUMesh mesh;
		mesh.Bounds = model.Bounds;

		// Resolve every image this model references to a shared texture asset and collect their
		// SRV indices. Images that came from a standalone file are keyed by that file's path, so a
		// texture used by two different models is uploaded once
		std::vector<DescriptorIndex> imageSRVs;
		imageSRVs.reserve(model.Images.size());

		AssetCache<GPUTexture>& textures = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>();

		GfxDevice::UploadContext& upload = device.GetUploadContext();
		upload.Begin();  // outer batch: each texture's own Begin/End nests into this one

		for (const ImageAsset& img : model.Images)
		{
			// Pixels are only carried inline for images embedded in the glTF; anything else is
			// decoded by the cache from img.Path
			const GPUTextureHandle handle = img.Pixels.empty()
				? textures.Acquire(device, img.Path)
				: textures.AcquireFrom(device, img.Path, img);

			const GPUTexture* texture = textures.Get(handle);
			imageSRVs.push_back(texture ? texture->SRVIndex : INVALID_DESCRIPTOR);

			if (texture)
				mesh.Textures.push_back(handle);
		}

		// --- Build one GPUMaterialData per model material (shared across however many submeshes use it) ---
		auto lookupSRV = [&](const Material& mat, DescriptorIndex Material::* textureIdxMember) -> DescriptorIndex
		{
			// .* dereferences pointer to member, giving the value of the member for this material
			const u32 imgIdx = mat.*textureIdxMember;
			return imgIdx < imageSRVs.size() ? imageSRVs[imgIdx] : INVALID_DESCRIPTOR;
		};

		std::vector<GPUMaterialData> materials;
		materials.reserve(model.Materials.size() + 1);
		for (const Material& mat : model.Materials)
		{
			const DescriptorIndex albedoSRV = lookupSRV(mat, &Material::BaseColorTextureIdx);
			materials.emplace_back(GPUMaterialData
			{
				.BaseColorFactor        = mat.MaterialConstants.BaseColorFactor,
				.EmissiveFactor         = Vector4(mat.MaterialConstants.EmissiveFactor.x, mat.MaterialConstants.EmissiveFactor.y, mat.MaterialConstants.EmissiveFactor.z, 0.0f),
				.MetallicFactor         = mat.MaterialConstants.MetallicFactor,
				.RoughnessFactor        = mat.MaterialConstants.RoughnessFactor,
				.NormalScale            = mat.MaterialConstants.NormalScale,
				.OcclusionStrength      = mat.MaterialConstants.OcclusionStrength,
				.AlphaCutoff            = mat.MaterialConstants.AlphaCutoff,
				.BaseColorIndex         = albedoSRV != INVALID_DESCRIPTOR ? albedoSRV : fallbackAlbedoSRV,
				.MetallicRoughnessIndex = lookupSRV(mat, &Material::MetallicRoughnessTextureIdx),
				.NormalIndex            = lookupSRV(mat, &Material::NormalTextureIdx),
				.OcclusionIndex         = lookupSRV(mat, &Material::OcclusionTextureIdx),
				.EmissiveIndex          = lookupSRV(mat, &Material::EmissiveTextureIdx),
			});
		}

		// Fallback entry for submeshes with no (or an out-of-range) material index.
		const u32 defaultMaterialIndex = static_cast<u32>(materials.size());
		materials.emplace_back(GPUMaterialData
		{
			.BaseColorFactor        = Vector4::One,
			.EmissiveFactor         = Vector4::Zero,
			.MetallicFactor         = 1.0f,
			.RoughnessFactor        = 1.0f,
			.NormalScale            = 1.0f,
			.OcclusionStrength      = 1.0f,
			.AlphaCutoff            = 0.5f,
			.BaseColorIndex         = fallbackAlbedoSRV,
			.MetallicRoughnessIndex = INVALID_DESCRIPTOR,
			.NormalIndex            = INVALID_DESCRIPTOR,
			.OcclusionIndex         = INVALID_DESCRIPTOR,
			.EmissiveIndex          = INVALID_DESCRIPTOR,
		});

		// Flatten all mesh vertices and indices into a single pair of buffers.
		std::vector<Vertex> vertices;
		std::vector<u32>    indices;

		// --- Create GPU buffers ---
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
					.IndexCount    = sub.IndexCount,
					.IndexOffset   = baseIndex  + sub.IndexOffset,
					.VertexOffset  = static_cast<i32>(baseVertex + sub.VertexOffset),
					.MaterialIndex = validMaterial ? static_cast<u32>(sub.MaterialIndex) : defaultMaterialIndex,
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

		const u64 matSize = materials.size() * sizeof(GPUMaterialData);
		mesh.MaterialBuffer = device.CreateBuffer(BufferDesc
		{
			.Size      = matSize,
			.Stride    = sizeof(GPUMaterialData),
			.Usage     = BufferUsage::Storage | BufferUsage::CopyDest,
			.Memory    = MemoryUsage::GpuOnly,
			.DebugName = "Mesh Materials",
		});

		// Fill the GPU-only buffers with their data	
		upload.UploadBuffer(mesh.VertexBuffer,   vertices.data(),  vbSize);
		upload.UploadBuffer(mesh.IndexBuffer,    indices.data(),   ibSize);
		upload.UploadBuffer(mesh.MaterialBuffer, materials.data(), matSize);

		// Upload complete
		upload.End();

		mesh.VertexBufferSRV = device.Get(mesh.VertexBuffer)->SRVIndex;
		assert(mesh.VertexBufferSRV != INVALID_DESCRIPTOR && "Vertex buffer has no bindless SRV");

		mesh.MaterialBufferSRV = device.Get(mesh.MaterialBuffer)->SRVIndex;
		assert(mesh.MaterialBufferSRV != INVALID_DESCRIPTOR && "Material buffer has no bindless SRV");

		KOI_LOG_DEBUG("GPUMesh uploaded: {} draws, {} verts, {} indices, {} materials, {} textures",
		              mesh.DrawItems.size(), vertices.size(), indices.size(), materials.size(), mesh.Textures.size());

		return mesh;
	}

	void GPUMesh::Release(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		device.Destroy(VertexBuffer);
		device.Destroy(IndexBuffer);
		device.Destroy(MaterialBuffer);

		// Each texture is destroyed only once every mesh using it has released it
		AssetCache<GPUTexture>& textures = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUTexture>();
		for (GPUTextureHandle handle : Textures)
		{
			textures.Release(device, handle);
		}

		Textures.clear();
		DrawItems.clear();
		VertexBufferSRV   = INVALID_DESCRIPTOR;
		MaterialBufferSRV = INVALID_DESCRIPTOR;
	}
}
