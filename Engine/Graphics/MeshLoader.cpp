#include "Graphics/MeshLoader.h"
#include "Core/Logger.h"
#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

namespace koi
{
	void LoadMaterials(const fastgltf::Asset& asset);
	void LoadMeshes(const fastgltf::Asset& asset, std::vector<Mesh>& outMeshes);
	bool LoadPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, SubMesh& outSubMesh, std::string_view name);

	std::optional<ModelAsset> MeshLoader::Load(const fs::path& path)
	{
		KOI_LOG_INFO("Loading mesh: {}", path.string());

		fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_variants
			| fastgltf::Extensions::KHR_texture_transform
			| fastgltf::Extensions::KHR_mesh_quantization);

		auto data = fastgltf::GltfDataBuffer::FromPath(path);

		if (data.error() != fastgltf::Error::None)
		{
			KOI_LOG_ERROR("fastgltf: failed to read file: {}", fastgltf::getErrorMessage(data.error()));
			return std::nullopt;
		}

		constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
			| fastgltf::Options::LoadExternalImages
			| fastgltf::Options::GenerateMeshIndices;

		fastgltf::GltfType type = fastgltf::determineGltfFileType(data.get());

		const fs::path basePath = path.parent_path();

		// Load GLTF or GLB
		auto loadResult = (type == fastgltf::GltfType::glTF)
			? parser.loadGltf(data.get(), basePath, options)
			: parser.loadGltfBinary(data.get(), basePath, options);

		if (loadResult.error() != fastgltf::Error::None)
		{
			KOI_LOG_ERROR("fastgltf: failed to parse: {}", fastgltf::getErrorMessage(loadResult.error()));
			return std::nullopt;
		}

		 const fastgltf::Asset& asset = loadResult.get();

		 ModelAsset model{ .Name = path.stem().string() };

		 LoadMaterials(asset);
		 LoadMeshes(asset, model.Meshes);

		 return model;
	}

	void LoadMaterials(const fastgltf::Asset& asset)
	{
		for (const fastgltf::Material& gltfMaterial : asset.materials)
		{
			Material m;
			m.Name = gltfMaterial.name.empty() ? "<Unnamed Material>" : gltfMaterial.name;
			KOI_LOG_WARN("GLTF material implementation is incomplete!");
		}
	}

	void LoadMeshes(const fastgltf::Asset& asset, std::vector<Mesh>& outMeshes)
	{
		std::vector<Mesh> meshes;
		for (const fastgltf::Mesh& gltfMesh : asset.meshes)
		{
			Mesh m;
			m.Name = gltfMesh.name.empty() ? "<Unnamed Mesh>" : gltfMesh.name;

			for (const fastgltf::Primitive& primitive : gltfMesh.primitives)
			{
				if (primitive.type != fastgltf::PrimitiveType::Triangles)
				{
					KOI_LOG_WARN("Primitive of mesh ({}) is not triangulated!", m.Name);
					continue;
				}

				SubMesh sub;
				if (LoadPrimitive(asset, primitive, sub, m.Name))
				{
					m.Bounds.Expand(sub.Bounds);
					m.SubMeshes.push_back(std::move(sub));
				}
			}

			outMeshes.push_back(std::move(m));
		}
	}

	bool LoadPrimitive(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive, SubMesh& outSubMesh, std::string_view name)
	{
		// [NOTE]: Assuming that material indices are unsigned
		outSubMesh.MaterialIndex = static_cast<u32>(primitive.materialIndex.value_or(-1));

		const fastgltf::Attribute* posAttrib = primitive.findAttribute("POSITION");
		if (!posAttrib || posAttrib == primitive.attributes.end())
		{
			KOI_LOG_WARN("Mesh ({}) does not have a POSITION attribute!", name.data());
			return false;
		}

		const fastgltf::Accessor& posAcc = asset.accessors[posAttrib->accessorIndex];
		const u32 vertexCount = static_cast<u32>(posAcc.count);

		outSubMesh.Vertices.resize(vertexCount);

		// Read position
		fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
			asset, posAcc,
			[&](const fastgltf::math::fvec3& position, size_t index)
			{
				const math::float3 p(position[0], position[1], position[2]);

				outSubMesh.Vertices[index].Position = p;
				outSubMesh.Bounds.Expand(p);
			});

		// Normals
		if (const fastgltf::Attribute* normAttrib = primitive.findAttribute("NORMAL"); normAttrib != primitive.attributes.end())
		{
			const fastgltf::Accessor& normAcc = asset.accessors[normAttrib->accessorIndex];

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
				asset, normAcc,
				[&](const fastgltf::math::fvec3& normal, size_t index)
				{
					outSubMesh.Vertices[index].Normal = math::float3(normal[0], normal[1], normal[2]);
				});
		}
		else
		{
			KOI_LOG_WARN("Mesh ({}) does not have a NORMAL attribute!", name.data());
		}

		// UV
		if (const fastgltf::Attribute* uvAttrib = primitive.findAttribute("TEXCOORD_0"); uvAttrib != primitive.attributes.end())
		{
			const fastgltf::Accessor& uvAcc = asset.accessors[uvAttrib->accessorIndex];

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
				asset, uvAcc,
				[&](const fastgltf::math::fvec2& uv, size_t index)
				{
					outSubMesh.Vertices[index].UV = math::float2(uv[0], uv[1]);
				});
		}
		else
		{
			KOI_LOG_WARN("Mesh ({}) does not have a TEXCOORD_0 attribute!", name.data());
		}

		// Tangents
		bool hasTangent = false;
		if (const fastgltf::Attribute* tangentAttrib = primitive.findAttribute("TANGENT"); tangentAttrib != primitive.attributes.end())
		{
			const fastgltf::Accessor& tangentAcc = asset.accessors[tangentAttrib->accessorIndex];

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
				asset, tangentAcc,
				[&](const fastgltf::math::fvec4& tangent, size_t index)
				{
					outSubMesh.Vertices[index].Tangent = math::float4(tangent[0], tangent[1], tangent[2], tangent[3]);
				});

			hasTangent = true;
		}
		else
		{
			KOI_LOG_WARN("Mesh ({}) does not have a TANGENT attribute!", name.data());
		}

		// Indices
		if (primitive.indicesAccessor.has_value())
		{
			const fastgltf::Accessor& idxAcc = asset.accessors[primitive.indicesAccessor.value()];
			outSubMesh.Indices.resize(idxAcc.count);

			fastgltf::iterateAccessorWithIndex<u32>(
				asset, idxAcc,
				[&](u32 index, size_t index2)
				{
					outSubMesh.Indices[index2] = index;
				});
		}
		else
		{
			// Non-indexed: generate sequential indices
			outSubMesh.Indices.reserve(vertexCount);
			for (u32 i = 0; i < vertexCount; ++i)
			{
				outSubMesh.Indices[i] = i;
			}
		}

		if (!hasTangent)
		{
			// Since no tangents were found, generate them
			//TODO: MikkTSpace tangent generation
			for (u32 i = 0; i < vertexCount; ++i)
			{
				outSubMesh.Vertices[i].Tangent = math::float4(1.0f, 0.0f, 0.0f, 0.0f);  // UnitX
			}
		}

		return true;
	}
}
