#include "Koi/Editor/Assets/MeshLoader.h"
#include "Koi/Engine/Assets/AssetData.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "Koi/Engine/Math/Math.h"
#include "stb_image.h"
#include <cstring>
#include <format>
#include <stdexcept>

namespace koi::editor
{
	namespace
	{
		// fastgltf stores matrices column-major (column-vector convention, v' = M * v);
		// SimpleMath stores them row-major (row-vector convention, v' = v * M). The transpose
		// and the storage-order difference cancel out, so the raw floats copy straight across
		Matrix ToMatrix(const fastgltf::math::fmat4x4& m)
		{
			Matrix out;
			std::memcpy(&out, m.data(), sizeof(f32) * 16);
			return out;
		}

		struct Loader
		{
			fastgltf::Asset* Asset = nullptr;
			fs::path BasePath;
			fs::path SourcePath;                // the glTF itself; used to key images embedded inside it
			std::string VirtualPath;            // the glTF's VFS path
			std::string VirtualBase;            // ...and its parent, for keying sibling image files
			bool DecodeEmbeddedImages = false;  // import only: the runtime reads extracted files instead

			// glTF image index -> index into ModelAsset::Images
			std::unordered_map<u32, u32> LoadedImages;

			std::unique_ptr<ModelAsset> BuildModelAsset();
			void LoadMeshes(ModelAsset& model);
			void AppendMesh(ModelAsset& model, fastgltf::Mesh& gltfMesh, const Matrix& worldXform);
			void LoadMaterials(ModelAsset& model);
			u32 LoadTexture(ModelAsset& model, u32 textureIndex);
		};
	}

	std::unique_ptr<ModelAsset> MeshLoader::Load(const fs::path& modelPath, bool decodeEmbeddedImages)
	{
		ZoneScoped;

		fs::path path;
		std::error_code ec;
		if (modelPath.is_absolute() && fs::exists(modelPath, ec))
		{
			path = modelPath;  // a file being imported from anywhere on disk
		}
		else if (std::optional<fs::path> resolveResult = VirtualFileSystem::Get().TryResolveToDisk(modelPath.string()))
		{
			path = *resolveResult;
		}
		else
		{
			KOI_LOG(Import, Error, "Failed to resolve path: {}", modelPath.string().c_str());
			return nullptr;
		}

		KOI_LOG(Import, Debug, "Loading mesh: {}", path.string().c_str());

		fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_variants
			| fastgltf::Extensions::KHR_texture_transform
			| fastgltf::Extensions::KHR_mesh_quantization);

		auto data = fastgltf::GltfDataBuffer::FromPath(path);
		if (data.error() != fastgltf::Error::None)
		{
			KOI_LOG(Import, Error, "fastgltf: failed to read file: {}", fastgltf::getErrorMessage(data.error()));
			return nullptr;
		}

		const std::string virtualPath = modelPath.generic_string();
		const size_t       lastSlash   = virtualPath.find_last_of('/');

		Loader loader
		{
			.BasePath    = path.parent_path(),
			.SourcePath  = path,
			.VirtualPath = virtualPath,
			.VirtualBase = lastSlash == std::string::npos ? virtualPath : virtualPath.substr(0, lastSlash),
			.DecodeEmbeddedImages = decodeEmbeddedImages,
		};

		// Deliberately no LoadExternalImages: it would pull URI-referenced images into memory as
		// embedded blobs, and then two models sharing a texture file could no longer be recognised
		// as the same asset. External images stay as URIs and are decoded by the texture AssetCache.
		constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
			| fastgltf::Options::GenerateMeshIndices;

		auto loadResult = parser.loadGltf(data.get(), loader.BasePath, options);
		if (!loadResult)
		{
			KOI_LOG(Import, Error, "fastgltf: failed to load file: {}", fastgltf::getErrorMessage(loadResult.error()).data());
			return nullptr;
		}

		loader.Asset = &loadResult.get();
		std::unique_ptr<ModelAsset> model = loader.BuildModelAsset();

		model->SourcePath = path.string();

		KOI_LOG(Import, Debug, "Loaded mesh '{}': {} mesh(es), {} material(s), {} image(s)",
			path.filename().string(), model->Meshes.size(), model->Materials.size(), model->Images.size());

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
	}

	void Loader::LoadMeshes(ModelAsset& model)
	{
		ZoneScoped;

		if (!Asset->scenes.empty())
		{
			const size_t sceneIndex = Asset->defaultScene.value_or(0);
			fastgltf::iterateSceneNodes(*Asset, sceneIndex, fastgltf::math::fmat4x4(),
				[&](fastgltf::Node& node, const fastgltf::math::fmat4x4& worldMatrix)
				{
					if (!node.meshIndex.has_value()) return;
					AppendMesh(model, Asset->meshes[*node.meshIndex], ToMatrix(worldMatrix));
				});
		}
		else
		{
			KOI_LOG(Import, Warn, "glTF has no scenes, emitting all meshes without node transforms");
			for (fastgltf::Mesh& gltfMesh : Asset->meshes)
			{
				AppendMesh(model, gltfMesh, Matrix::Identity);
			}
		}
	}

	void Loader::AppendMesh(ModelAsset& model, fastgltf::Mesh& gltfMesh, const Matrix& worldXform)
	{
		ZoneScoped;
		Mesh mesh;
		mesh.Name = gltfMesh.name;

		// Normals use the inverse-transpose so non-uniform scale does not shear them
		// A mirroring transform (negative determinant) flips tangent-space handedness
		const Matrix normalXform    = worldXform.Invert().Transpose();
		const f32    handednessFlip = worldXform.Determinant() < 0.0f ? -1.0f : 1.0f;

		std::vector<Vertex> allVertices;
		std::vector<u32> allIndices;

		// For-each primitive
		for (fastgltf::Primitive& primitive : gltfMesh.primitives)
		{
			SubMesh sub;
			sub.VertexOffset = static_cast<u32>(allVertices.size());
			sub.IndexOffset = static_cast<u32>(allIndices.size());
			sub.MaterialIndex = static_cast<i32>(primitive.materialIndex.value_or(-1));

			auto posAccessor = primitive.findAttribute("POSITION");
			if (posAccessor == primitive.attributes.end())
			{
				KOI_LOG(Import, Warn, "Primitive in mesh '{}' has no POSITION attribute, skipping", mesh.Name);
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
					vertices[i].Position = Vector3(pos.x(), pos.y(), pos.z());
				});

			// Normals
			if (auto normAttr = primitive.findAttribute("NORMAL"); normAttr != primitive.attributes.end())
			{
				fastgltf::Accessor& acc = Asset->accessors[normAttr->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(*Asset, acc,
					[&](const fastgltf::math::fvec3& n, std::size_t i)
					{
						vertices[i].Normal = Vector3(n.x(), n.y(), n.z());
					});
			}
			else
			{
				KOI_LOG(Import, Trace, "Primitive in mesh '{}' has no NORMAL attribute", mesh.Name);
			}

			// UV
			if (auto uvAttr = primitive.findAttribute("TEXCOORD_0"); uvAttr != primitive.attributes.end())
			{
				fastgltf::Accessor& acc = Asset->accessors[uvAttr->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(*Asset, acc,
					[&](const fastgltf::math::fvec2& uv, std::size_t i)
					{
						vertices[i].UV = Vector2(uv.x(), uv.y());
					});
			}
			else
			{
				KOI_LOG(Import, Trace, "Primitive in mesh '{}' has no TEXCOORD_0 attribute", mesh.Name);
			}

			// Tangent
			bool hasTangent = false;
			if (auto tanAttr = primitive.findAttribute("TANGENT"); tanAttr != primitive.attributes.end())
			{
				fastgltf::Accessor& acc = Asset->accessors[tanAttr->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(*Asset, acc,
					[&](const fastgltf::math::fvec4& t, std::size_t i)
					{
						vertices[i].Tangent = Vector4(t.x(), t.y(), t.z(), t.w());
					});
				hasTangent = true;
			}
			else
			{
				KOI_LOG(Import, Trace, "Primitive in mesh '{}' has no TANGENT attribute; generating placeholders", mesh.Name);
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
					vertices[i].Tangent = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
				}
			}

			// Bake this node's world transform into the vertices
			for (u32 i = 0; i < vertexCount; ++i)
			{
				Vertex& v = vertices[i];

				v.Position = Vector3::Transform(v.Position, worldXform);

				Vector3 normal = Vector3::TransformNormal(v.Normal, normalXform);
				normal.Normalize();
				v.Normal = normal;

				Vector3 tangent = Vector3::TransformNormal(Vector3(v.Tangent.x, v.Tangent.y, v.Tangent.z), worldXform);
				tangent.Normalize();
				v.Tangent = Vector4(tangent.x, tangent.y, tangent.z, v.Tangent.w * handednessFlip);
			}

			if (vertexCount > 0)
			{
				BoundingBox::CreateFromPoints(sub.Bounds, vertexCount,
					reinterpret_cast<const DirectX::XMFLOAT3*>(&vertices[0].Position), sizeof(Vertex));
			}

			math::MergeBounds(mesh.Bounds, sub.Bounds);
			mesh.SubMeshes.push_back(std::move(sub));
		}

		mesh.VertexCount = static_cast<u32>(allVertices.size());
		mesh.IndexCount  = static_cast<u32>(allIndices.size());
		mesh.Vertices    = std::move(allVertices);
		mesh.Indices     = std::move(allIndices);

		KOI_LOG(Import, Trace, "Mesh '{}': {} submesh(es), {} verts, {} indices",
			mesh.Name, mesh.SubMeshes.size(), mesh.VertexCount, mesh.IndexCount);

		math::MergeBounds(model.Bounds, mesh.Bounds);
		model.Meshes.push_back(std::move(mesh));
	}

	void Loader::LoadMaterials(ModelAsset& model)
	{
		ZoneScoped;
		for (fastgltf::Material& gltfMat : Asset->materials)
		{
			ImportMaterial mat;
			mat.Name = gltfMat.name;
			mat.DoubleSided = gltfMat.doubleSided;

			switch (gltfMat.alphaMode)
			{
				using enum fastgltf::AlphaMode;
			case Mask:  mat.Mode = ImportMaterial::AlphaMode::Mask;  break;
			case Blend: mat.Mode = ImportMaterial::AlphaMode::Blend; break;
			default:    mat.Mode = ImportMaterial::AlphaMode::Opaque; break;
			}

			mat.MaterialConstants.AlphaCutoff = gltfMat.alphaCutoff;

			// PBR
			auto& pbr = gltfMat.pbrData;
			mat.MaterialConstants.BaseColorFactor = Vector4(
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
				mat.MaterialConstants.EmissiveFactor = Vector3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1], gltfMat.emissiveFactor[2]);
			}

			model.Materials.push_back(std::move(mat));
		}
	}

	u32 Loader::LoadTexture(ModelAsset& model, u32 textureIndex)
	{
		ZoneScoped;

		fastgltf::Texture& gltfTexture = Asset->textures[textureIndex];
		if (!gltfTexture.imageIndex.has_value())
		{
			return u32(-1);
		}

		const u32 imageIndex = static_cast<u32>(*gltfTexture.imageIndex);
		if (auto it = LoadedImages.find(imageIndex); it != LoadedImages.end())
			return it->second;

		const u32 idx = static_cast<u32>(model.Images.size());
		fastgltf::Image& gltfImage = Asset->images[imageIndex];

		ImageAsset img{};

		if (!DecodeEmbeddedImages && !std::holds_alternative<fastgltf::sources::URI>(gltfImage.data))
		{
			// Import extracts these to files; until then the material stays untextured
			return u32(-1);
		}

		if (std::holds_alternative<fastgltf::sources::URI>(gltfImage.data))
		{
			// Standalone file: record where it lives and leave Pixels empty. The texture
			// AssetCache decodes it, so the same file referenced by another model resolves to
			// the same GPU texture instead of a second copy
			auto& uri = std::get<fastgltf::sources::URI>(gltfImage.data);

			if (uri.fileByteOffset != 0 || !uri.uri->isLocalPath())
			{
				throw std::runtime_error(std::format("image {} isn't a plain local file", uri.uri->string()));
			}

			// Using virtual path to prevent texture loading process try to query from VFS path
			img.Path = VirtualBase + "/" + uri.uri->fspath().generic_string();
			img.Name = uri.uri->fspath().filename().string();
		}
		else
		{
			// Embedded: there is no file to point at, so decode here and hand the pixels to the
			// cache under a synthetic key derived from the container it came out of
			const stbi_uc* encoded = nullptr;
			size_t encodedSize     = 0;

			if (std::holds_alternative<fastgltf::sources::Array>(gltfImage.data))
			{
				auto& array = std::get<fastgltf::sources::Array>(gltfImage.data);
				encoded     = reinterpret_cast<const stbi_uc*>(array.bytes.data());
				encodedSize = array.bytes.size();
			}
			else if (std::holds_alternative<fastgltf::sources::BufferView>(gltfImage.data))
			{
				auto& source = std::get<fastgltf::sources::BufferView>(gltfImage.data);

				fastgltf::BufferView& view   = Asset->bufferViews[source.bufferViewIndex];
				fastgltf::Buffer&     buffer = Asset->buffers[view.bufferIndex];

				if (std::holds_alternative<fastgltf::sources::Array>(buffer.data))
				{
					// The view is a window into the buffer
					auto& array = std::get<fastgltf::sources::Array>(buffer.data);
					encoded     = reinterpret_cast<const stbi_uc*>(array.bytes.data()) + view.byteOffset;
					encodedSize = view.byteLength;
				}
			}
			else
			{
				throw std::runtime_error(std::format("image {} has an unsupported data source", imageIndex));
			}

			if (!encoded || encodedSize == 0)
			{
				KOI_LOG(Import, Warn, "Texture image index {} has no readable data", imageIndex);
				return u32(-1);
			}

			stbi_uc* pixels = stbi_load_from_memory(
				encoded, static_cast<i32>(encodedSize),
				&img.Width, &img.Height, &img.Channels, 4);

			if (!pixels)
			{
				KOI_LOG(Import, Warn, "Failed to decode embedded texture image index {}", imageIndex);
				return u32(-1);
			}

			KOI_ASSERT(img.Width > 0 && img.Height > 0);

			// stbi was asked for 4 components, so the returned data is always RGBA
			// regardless of the file's native channel count.
			img.Channels = 4;
			const size_t byteCount = static_cast<size_t>(img.Width) * img.Height * 4;
			img.Pixels.assign(pixels, pixels + byteCount);
			stbi_image_free(pixels);

			img.Path = fmt::format("{}#image{}", VirtualPath, imageIndex);
			img.Name = gltfTexture.name.empty() ? fmt::format("Image_{}", imageIndex) : std::string(gltfTexture.name);
		}

		model.Images.push_back(std::move(img));
		LoadedImages.emplace(imageIndex, idx);
		return idx;
	}
}
