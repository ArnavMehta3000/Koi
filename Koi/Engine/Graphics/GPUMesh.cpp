#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/RHI/Device.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Logging/Logger.h"
#include <cassert>

namespace koi
{
	GPUMesh GPUMesh::Upload(GfxDevice& device, const ModelAsset& model, DescriptorIndex fallbackAlbedoSRV)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		GfxDevice::UploadContext& upload = device.GetUploadContext();

		GPUMesh mesh;

		// Upload all images and collect their SRV indices.
		std::vector<DescriptorIndex> imageSRVs;
		imageSRVs.reserve(model.Images.size());

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
#pragma region Get SubMesh Material/Texture Data
				std::optional<DescriptorIndex> albedoSRV = [&]() -> std::optional<DescriptorIndex>
				{
					if (sub.MaterialIndex >= 0 && static_cast<size_t>(sub.MaterialIndex) < model.Materials.size())
					{
						const Material& mat = model.Materials[sub.MaterialIndex];
						const u32 imgIdx = mat.BaseColorTextureIdx;
						if (imgIdx < imageSRVs.size())
							return imageSRVs[imgIdx];
					}
					return std::nullopt;
				}();

				std::optional<DescriptorIndex> metallicRoughnessSRV = [&]() -> std::optional<DescriptorIndex>
				{
					if (sub.MaterialIndex >= 0 && static_cast<size_t>(sub.MaterialIndex) < model.Materials.size())
					{
						const Material& mat = model.Materials[sub.MaterialIndex];
						const u32 imgIdx = mat.MetallicRoughnessTextureIdx;
						if (imgIdx < imageSRVs.size())
							return imageSRVs[imgIdx];
					}
					return std::nullopt;
				}();

				std::optional<DescriptorIndex> normalSRV = [&]() -> std::optional<DescriptorIndex>
				{
					if (sub.MaterialIndex >= 0 && static_cast<size_t>(sub.MaterialIndex) < model.Materials.size())
					{
						const Material& mat = model.Materials[sub.MaterialIndex];
						const u32 imgIdx = mat.NormalTextureIdx;
						if (imgIdx < imageSRVs.size())
							return imageSRVs[imgIdx];
					}
					return std::nullopt;
				}();

				std::optional<DescriptorIndex> occlusionSRV = [&]() -> std::optional<DescriptorIndex>
				{
					if (sub.MaterialIndex >= 0 && static_cast<size_t>(sub.MaterialIndex) < model.Materials.size())
					{
						const Material& mat = model.Materials[sub.MaterialIndex];
						const u32 imgIdx = mat.OcclusionTextureIdx;
						if (imgIdx < imageSRVs.size())
							return imageSRVs[imgIdx];
					}
					return std::nullopt;
				}();

				std::optional<DescriptorIndex> emissiveSRV = [&]() -> std::optional<DescriptorIndex>
				{
					if (sub.MaterialIndex >= 0 && static_cast<size_t>(sub.MaterialIndex) < model.Materials.size())
					{
						const Material& mat = model.Materials[sub.MaterialIndex];
						const u32 imgIdx = mat.EmissiveTextureIdx;
						if (imgIdx < imageSRVs.size())
							return imageSRVs[imgIdx];
					}
					return std::nullopt;
				}();
#pragma endregion



				mesh.DrawItems.push_back(GPUMeshDraw
				{
					.IndexCount           = sub.IndexCount,
					.IndexOffset          = baseIndex  + sub.IndexOffset,
					.VertexOffset         = static_cast<i32>(baseVertex + sub.VertexOffset),
					.BaseColorSRV         = albedoSRV.value_or(fallbackAlbedoSRV),
					.MetallicRoughnessSRV = metallicRoughnessSRV.value_or(INVALID_DESCRIPTOR),
					.NormalSRV            = normalSRV.value_or(INVALID_DESCRIPTOR),
					.OcclusionSRV         = occlusionSRV.value_or(INVALID_DESCRIPTOR),
					.EmissiveSRV          = emissiveSRV.value_or(INVALID_DESCRIPTOR),
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

		upload.UploadBuffer(mesh.VertexBuffer, vertices.data(), vbSize);
		upload.UploadBuffer(mesh.IndexBuffer,  indices.data(),  ibSize);
		upload.End();

		mesh.VertexBufferSRV = device.Get(mesh.VertexBuffer)->SRVIndex;
		assert(mesh.VertexBufferSRV != INVALID_DESCRIPTOR && "Vertex buffer has no bindless SRV");

		KOI_LOG_DEBUG("GPUMesh uploaded: {} draws, {} verts, {} indices, {} textures",
		              mesh.DrawItems.size(), vertices.size(), indices.size(), mesh.Textures.size());

		return mesh;
	}

	void GPUMesh::Release(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		device.Destroy(VertexBuffer);
		device.Destroy(IndexBuffer);

		for (TextureHandle t : Textures)
		{
			device.Destroy(t);
		}

		Textures.clear();
		DrawItems.clear();
		VertexBufferSRV = INVALID_DESCRIPTOR;
	}
}
