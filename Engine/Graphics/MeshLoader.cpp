#include "Graphics/MeshLoader.h"
#include "Core/Logger.h"
#include "Core/Profiling.h"
#include "Core/VirtualFileSystem.h"
#include "Memory/MemoryTracker.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <string>

namespace koi
{
	namespace
	{
		struct Loader
		{
			fastgltf::Asset* Asset = nullptr;
			fs::path BasePath;

			std::unique_ptr<ModelAsset> BuildModelAsset();
			void LoadMeshes(ModelAsset& model);
			void LoadMaterials(ModelAsset& model);
			u32 LoadTexture(ModelAsset& model, u32 textureIndex);
		};
	}

	std::unique_ptr<ModelAsset> MeshLoader::Load(const fs::path& modelPath)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		ZoneScoped;

		fs::path path;
		if (std::optional<fs::path> resolveResult = VirtualFileSystem::Get().TryResolveToDisk(modelPath.string()))
		{
			path = *resolveResult;
		}
		else
		{
			KOI_LOG_ERROR("Failed to resolve path: {}", modelPath.string().c_str());
			return nullptr;
		}

		KOI_LOG_INFO("Loading mesh: {}", path.string().c_str());

		fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_variants
			| fastgltf::Extensions::KHR_texture_transform
			| fastgltf::Extensions::KHR_mesh_quantization);

		auto data = fastgltf::GltfDataBuffer::FromPath(path);
		if (data.error() != fastgltf::Error::None)
		{
			KOI_LOG_ERROR("fastgltf: failed to read file: {}", fastgltf::getErrorMessage(data.error()));
			return nullptr;
		}

		Loader loader{ .BasePath = path.parent_path() };

		constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
			| fastgltf::Options::LoadExternalImages
			| fastgltf::Options::GenerateMeshIndices;

		auto loadResult = parser.loadGltf(data.get(), loader.BasePath, options);
		if (!loadResult)
		{
			KOI_LOG_ERROR("fastgltf: failed to load file: {}", fastgltf::getErrorMessage(loadResult.error()).data());
			return nullptr;
		}

		loader.Asset = &loadResult.get();
		std::unique_ptr<ModelAsset> model = loader.BuildModelAsset();

		model->SourcPath = path.string();


		return model;
	}

	std::unique_ptr<ModelAsset>Loader::BuildModelAsset()
	{
		ZoneScoped;
		auto model = std::make_unique<ModelAsset>();

		LoadMaterials(*model);
		LoadMeshes(*model);
		// Todo create samplers

		return model;
		return model;
	}

	void Loader::LoadMeshes(ModelAsset& model)
	{
		ZoneScoped;
		// For-each mesh
		for (fastgltf::Mesh& gltfMesh : Asset->meshes)
		{
			Mesh mesh;
			mesh.Name = gltfMesh.name;

			std::vector<Vertex> allVertices;
			std::vector<u32> allIndices;

			// For-each primitive
			for (fastgltf::Primitive& primitive : gltfMesh.primitives)
			{
				SubMesh sub;
				sub.VertexOffset = static_cast<u32>(allVertices.size());
				sub.IndexOffset = static_cast<u32>(allIndices.size());
				sub.MaterialIndex = static_cast<i32>(primitive.materialIndex.value_or(-1));

				fastgltf::Attribute* posAccessor = primitive.findAttribute("POSITION");
				if (!posAccessor || posAccessor == primitive.attributes.end())
				{
					KOI_LOG_WARN("Primitive in mesh '{}' has no POSITION attribute, skipping", mesh.Name);
					continue;
				}

				fastgltf::Accessor& posAcc = Asset->accessors[posAccessor->accessorIndex];
				const u32 vertexCount = static_cast<u32>(posAcc.count);

				allVertices.resize(allVertices.size() + vertexCount);
				Vertex* vertices = &allVertices[sub.VertexOffset];

				// Read positions
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*Asset, posAcc,
					[&](const fastgltf::math::fvec3& pos, std::size_t i)
					{
						vertices[i].Position = math::float3(pos.x(), pos.y(), pos.z());
						sub.Bounds.Expand(vertices[i].Position);
					});

				// Normals
				if (fastgltf::Attribute* normAttr = primitive.findAttribute("NORMAL"); normAttr != primitive.attributes.end())
				{
					fastgltf::Accessor& acc = Asset->accessors[normAttr->accessorIndex];
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*Asset, acc,
						[&](const fastgltf::math::fvec3& n, std::size_t i)
						{
							vertices[i].Normal = math::float3(n.x(), n.y(), n.z());
						});
				}
				else
				{
					KOI_LOG_WARN("Primitive in mesh '{}' has no NORMAL attribute", mesh.Name);
				}

				// UV
				if (fastgltf::Attribute* uvAttr = primitive.findAttribute("TEXCOORD_0"); uvAttr != primitive.attributes.end())
				{
					fastgltf::Accessor& acc = Asset->accessors[uvAttr->accessorIndex];
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(*Asset, acc,
						[&](const fastgltf::math::fvec2& uv, std::size_t i)
						{
							vertices[i].UV = math::float2(uv.x(), uv.y());
						});
				}
				else
				{
					KOI_LOG_WARN("Primitive in mesh '{}' has no TEXCOORD_0 attribute", mesh.Name);
				}

				// Tangent
				bool hasTangent = false;
				if (fastgltf::Attribute* tanAttr = primitive.findAttribute("TANGENT"); tanAttr != primitive.attributes.end())
				{
					fastgltf::Accessor& acc = Asset->accessors[tanAttr->accessorIndex];
					fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(*Asset, acc,
						[&](const fastgltf::math::fvec4& t, std::size_t i)
						{
							vertices[i].Tangent = math::float4(t.x(), t.y(), t.z(), t.w());
						});
					hasTangent = true;
				}
				else
				{
					KOI_LOG_WARN("Primitive in mesh '{}' has no TANGENT attribute", mesh.Name);
				}

				// Indices
				if (primitive.indicesAccessor.has_value())
				{
					fastgltf::Accessor& idxAcc = Asset->accessors[*primitive.indicesAccessor];
					sub.IndexCount = static_cast<u32>(idxAcc.count);
					allIndices.reserve(allIndices.size() + sub.IndexCount);

					fastgltf::iterateAccessor<u32>(*Asset, idxAcc,
						[&](u32 idx)
						{
							allIndices.push_back(idx);
						});
				}
				else
				{
					// Non-indexed: generate sequential indices
					sub.IndexCount = vertexCount;
					for (u32 i = 0; i < vertexCount; ++i)
					{
						allIndices.push_back(i);
					}
				}

				if (!hasTangent)
				{
					// Since no tangents were found, generate them
					//TODO: MikkTSpace tangent generation
					for (u32 i = 0; i < vertexCount; ++i)
					{
						vertices[i].Tangent = math::float4(1.0f, 0.0f, 0.0f, 0.0f);
					}
				}

				mesh.Bounds.Expand(sub.Bounds);
				mesh.SubMeshes.push_back(std::move(sub));
			}

			mesh.VertexCount = static_cast<u32>(allVertices.size());
			mesh.IndexCount = static_cast<u32>(allIndices.size());
			mesh.Vertices = std::move(allVertices);
			mesh.Indices = std::move(allIndices);

			model.Bounds.Expand(mesh.Bounds);
			model.Meshes.push_back(std::move(mesh));
		}
	}

	void Loader::LoadMaterials(ModelAsset& model)
	{
		ZoneScoped;
		for (fastgltf::Material& gltfMat : Asset->materials)
		{
			Material mat;
			mat.Name = gltfMat.name;
			mat.DoubleSided = gltfMat.doubleSided;

			switch (gltfMat.alphaMode)
			{
				using enum fastgltf::AlphaMode;
			case Mask:  mat.Mode = Material::AlphaMode::Mask;  break;
			case Blend: mat.Mode = Material::AlphaMode::Blend; break;
			default:    mat.Mode = Material::AlphaMode::Opaque; break;
			}

			mat.MaterialConstants.AlphaCutoff = gltfMat.alphaCutoff;

			// PBR
			auto& pbr = gltfMat.pbrData;
			mat.MaterialConstants.BaseColorFactor = math::float4(
				pbr.baseColorFactor[0], pbr.baseColorFactor[1],
				pbr.baseColorFactor[2], pbr.baseColorFactor[3]);

			mat.MaterialConstants.MetallicFactor = pbr.metallicFactor;
			mat.MaterialConstants.RoughnessFactor = pbr.roughnessFactor;

			if (pbr.baseColorTexture)
			{
				mat.BaseColorTextureIdx = LoadTexture(model, static_cast<u32>(pbr.baseColorTexture->textureIndex));
			}

			if (pbr.metallicRoughnessTexture)
			{
				mat.MetallicRoughnessTextureIdx = LoadTexture(model, static_cast<u32>(pbr.metallicRoughnessTexture->textureIndex));
			}

			if (gltfMat.normalTexture)
			{
				mat.NormalTextureIdx = LoadTexture(model, static_cast<u32>(gltfMat.normalTexture->textureIndex));
			}

			if (gltfMat.occlusionTexture)
			{
				mat.OcclusionTextureIdx = LoadTexture(model, static_cast<u32>(gltfMat.occlusionTexture->textureIndex));
				mat.MaterialConstants.OcclusionStrength = gltfMat.occlusionTexture->strength;
			}

			if (gltfMat.emissiveTexture)
			{
				mat.EmissiveTextureIdx = LoadTexture(model, static_cast<u32>(gltfMat.emissiveTexture->textureIndex));
				mat.MaterialConstants.EmissiveFactor = math::float3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);
			}

			model.Materials.push_back(std::move(mat));
		}
	}

	u32 Loader::LoadTexture(ModelAsset& model, u32 textureIndex)
	{
		ZoneScoped;
		u32 idx = static_cast<u32>(model.Images.size());

		fastgltf::Texture& gltfTexture = Asset->textures[textureIndex];
		if (!gltfTexture.imageIndex.has_value())
		{
			return u32(-1);
		}

		fastgltf::Image& gltfImage = Asset->images[*gltfTexture.imageIndex];

		ImageAsset img{};
		stbi_uc* pixels = nullptr;
		if (std::holds_alternative<fastgltf::sources::URI>(gltfImage.data))
		{
			auto& uri = std::get<fastgltf::sources::URI>(gltfImage.data);  // Load from filename

			assert(uri.fileByteOffset == 0);
			assert(uri.uri.isLocalPath());  // Needs to be a local file to be loaded

			fs::path fullPath = BasePath / uri.uri.fspath();
			pixels = stbi_load(fullPath.string().c_str(), &img.Width, &img.Height, &img.Channels, 4);
		}
		else if (std::holds_alternative<fastgltf::sources::Array>(gltfImage.data))  // Load from memory
		{
			auto& array = std::get<fastgltf::sources::Array>(gltfImage.data);
			pixels = stbi_load_from_memory(
				reinterpret_cast<const stbi_uc*>(array.bytes.data()),
				static_cast<i32>(array.bytes.size()),
				&img.Width, &img.Height, &img.Channels, 4);
		}
		else if (std::holds_alternative<fastgltf::sources::BufferView>(gltfImage.data))  // Load from buffer view
		{
			auto& bufferView = std::get<fastgltf::sources::BufferView>(gltfImage.data);

			fastgltf::BufferView& view = Asset->bufferViews[bufferView.bufferViewIndex];
			fastgltf::Buffer& buffer = Asset->buffers[view.bufferIndex];

			if (std::holds_alternative<fastgltf::sources::Array>(buffer.data))
			{
				auto& array = std::get<fastgltf::sources::Array>(buffer.data);
				pixels = stbi_load_from_memory(
					reinterpret_cast<const stbi_uc*>(array.bytes.data()),
					static_cast<i32>(array.bytes.size()),
					&img.Width, &img.Height, &img.Channels, 4);
			}
		}
		else
		{
			assert(!"Unknown image data type");
		}

		if (!pixels)
		{
			KOI_LOG_WARN("Failed to load texture image index {}", *gltfTexture.imageIndex);
			return u32(-1);
		}
		else
		{
			assert(img.Width > 0 && img.Height > 0);

			// stbi was asked for 4 components, so the returned data is always RGBA
			// regardless of the file's native channel count.
			img.Channels = 4;
			const size_t byteCount = static_cast<size_t>(img.Width) * img.Height * 4;
			img.Pixels.resize(byteCount);
			std::copy(pixels, pixels + byteCount, img.Pixels.data());

			stbi_image_free(pixels);
		}

		img.Name = (gltfTexture.name.empty()) ? fmt::format("Texture_{}", idx) : gltfTexture.name.data();
		model.Images.push_back(std::move(img));
		return idx;
	}
}
