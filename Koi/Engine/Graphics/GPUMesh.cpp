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
			auto handle = Engine::Get().GetRenderer().GetDefaultResources().WhiteTexture;
			Texture* tex = device.Get(handle);
			assert(tex && tex->Resource && "Default white texture is missing");
			return tex->SRVIndex;
		}();

		GPUMesh mesh;

		// Upload all images and collect their SRV indices
		std::vector<DescriptorIndex> imageSRVs;
		imageSRVs.reserve(model.Images.size());

		GfxDevice::UploadContext& upload = device.GetUploadContext();
		upload.Begin();

		// --- Create GPU textures --- 
		for (const ImageAsset& img : model.Images)
		{
			// Create & upload a GPU texture
			TextureHandle tex = device.CreateTexture(TextureDesc
			{
				.Width     = static_cast<u32>(img.Width),
				.Height    = static_cast<u32>(img.Height),
				.Format    = Format::R8G8B8A8_UNORM,
				.Usage     = TextureUsage::ShaderRead,
				.DebugName = img.Name.empty() ? "Mesh Texture" : std::string_view(img.Name),
			});
			upload.UploadTexture(tex, img.Pixels.data(), img.Pixels.size());

			// Use the handle to get the underlying GPU texture, then its SRV index
			if (Texture* texture = device.Get(tex); texture->Resource)
			{
				imageSRVs.push_back(texture->SRVIndex);

				// Cache the texture handle for this image
				mesh.Textures.push_back(tex);
			}
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
				.EmissiveFactor         = float4(mat.MaterialConstants.EmissiveFactor, 0.0f),
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
			.BaseColorFactor        = float4(1.0f, 1.0f, 1.0f, 1.0f),
			.EmissiveFactor         = float4(0.0f, 0.0f, 0.0f, 0.0f),
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

		for (TextureHandle t : Textures)
		{
			device.Destroy(t);
		}

		Textures.clear();
		DrawItems.clear();
		VertexBufferSRV   = INVALID_DESCRIPTOR;
		MaterialBufferSRV = INVALID_DESCRIPTOR;
	}
}
