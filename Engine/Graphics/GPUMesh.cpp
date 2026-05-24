#include "Graphics/GPUMesh.h"
#include "Graphics/RHI/Device.h"
#include "Memory/MemoryTracker.h"
#include "Core/Logger.h"
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

		for (const ImageAsset& img : model.Images)
		{
			TextureHandle tex = device.CreateTexture(TextureDesc
			{
				.Width     = static_cast<u32>(img.Width),
				.Height    = static_cast<u32>(img.Height),
				.Format    = Format::R8G8B8A8_UNORM,
				.Usage     = TextureUsage::ShaderRead,
				.DebugName = img.Name.empty() ? "Mesh Texture" : std::string_view(img.Name),
			});
			upload.UploadTexture(tex, img.Pixels.data(), img.Pixels.size());
			imageSRVs.push_back(device.Get(tex)->SRVIndex);
			mesh.Textures.push_back(tex);
		}

		// Flatten all mesh vertices and indices into a single pair of buffers.
		std::vector<Vertex> vertices;
		std::vector<u32>    indices;

		for (const Mesh& m : model.Meshes)
		{
			const u32 baseVertex = static_cast<u32>(vertices.size());
			const u32 baseIndex  = static_cast<u32>(indices.size());

			vertices.insert(vertices.end(), m.Vertices.begin(), m.Vertices.end());
			indices.insert(indices.end(),   m.Indices.begin(),  m.Indices.end());

			for (const SubMesh& sub : m.SubMeshes)
			{
				DescriptorIndex albedoSRV = fallbackAlbedoSRV;
				if (sub.MaterialIndex >= 0 && static_cast<size_t>(sub.MaterialIndex) < model.Materials.size())
				{
					const u32 imgIdx = model.Materials[sub.MaterialIndex].BaseColorTextureIdx;
					if (imgIdx < imageSRVs.size())
						albedoSRV = imageSRVs[imgIdx];
				}

				mesh.DrawItems.push_back(GPUMeshDraw
				{
					.IndexCount   = sub.IndexCount,
					.IndexOffset  = baseIndex  + sub.IndexOffset,
					.VertexOffset = static_cast<i32>(baseVertex + sub.VertexOffset),
					.BaseColorSRV = albedoSRV,
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
