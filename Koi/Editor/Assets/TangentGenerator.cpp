#include "Koi/Editor/Assets/TangentGenerator.h"
#include "Koi/Engine/Assets/MeshFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Resources/AssetRegistry.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <mikktspace.h>
#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace koi::editor
{
	namespace
	{
		struct MikkTSpaceGenerator
		{
			std::vector<Vertex> Corners;  // unindexed: three per face

			static MikkTSpaceGenerator& Self(const SMikkTSpaceContext* ctx)
			{
				return *static_cast<MikkTSpaceGenerator*>(ctx->m_pUserData);
			}

			static Vertex& Corner(const SMikkTSpaceContext* ctx, int iFace, int iVert)
			{
				return Self(ctx).Corners[static_cast<size_t>(iFace) * 3 + iVert];
			}

			static int GetNumFaces(const SMikkTSpaceContext* ctx)
			{
				return static_cast<int>(Self(ctx).Corners.size() / 3);
			}

			static int GetNumVerticesOfFace(const SMikkTSpaceContext*, const int) { return 3; }

			static void GetPosition(const SMikkTSpaceContext* ctx, float fvPosOut[], const int iFace, const int iVert)
			{
				const Vector3& position = Corner(ctx, iFace, iVert).Position;
				fvPosOut[0] = position.x;
				fvPosOut[1] = position.y;
				fvPosOut[2] = position.z;
			}

			static void GetNormal(const SMikkTSpaceContext* ctx, float fvNormOut[], const int iFace, const int iVert)
			{
				const Vector3& normal = Corner(ctx, iFace, iVert).Normal;
				fvNormOut[0] = normal.x;
				fvNormOut[1] = normal.y;
				fvNormOut[2] = normal.z;
			}

			static void GetCoord(const SMikkTSpaceContext* ctx, float fvTexcOut[], const int iFace, const int iVert)
			{
				const Vector2& uv = Corner(ctx, iFace, iVert).UV;
				fvTexcOut[0] = uv.x;
				fvTexcOut[1] = uv.y;
			}

			static void SetTSpaceBasic(const SMikkTSpaceContext* ctx, const float fvTangent[], const float fSign, const int iFace, const int iVert)
			{
				// glTF's V points down the image and its green channel up, so the bitangent is the other way round
				Corner(ctx, iFace, iVert).Tangent = Vector4(fvTangent[0], fvTangent[1], fvTangent[2], -fSign);
			}

			static SMikkTSpaceInterface* GetInterface()
			{
				static SMikkTSpaceInterface i
				{
					.m_getNumFaces          = GetNumFaces,
					.m_getNumVerticesOfFace = GetNumVerticesOfFace,
					.m_getPosition          = GetPosition,
					.m_getNormal            = GetNormal,
					.m_getTexCoord          = GetCoord,
					.m_setTSpaceBasic       = SetTSpaceBasic,
				};

				return &i;
			}
		};
	}

	bool TangentGenerator::Generate(std::vector<Vertex>& vertices, std::vector<u32>& indices)
	{
		ZoneScoped;

		if (indices.empty() || indices.size() % 3 != 0) return false;

		// Unindex: one vertex per triangle corner, so corners that shared a vertex can get different tangents
		MikkTSpaceGenerator gen;
		gen.Corners.reserve(indices.size());
		for (const u32 index : indices)
		{
			if (index >= vertices.size()) return false;
			gen.Corners.push_back(vertices[index]);
		}

		const SMikkTSpaceContext ctx{ .m_pInterface = MikkTSpaceGenerator::GetInterface(), .m_pUserData = &gen };
		if (!::genTangSpaceDefault(&ctx)) return false;

		// Weld: corners with identical bytes become one vertex again
		std::vector<Vertex> welded;
		std::vector<u32>    weldedIndices;
		std::unordered_map<std::string_view, u32> indexOf;  // keys view into gen.Corners
		weldedIndices.reserve(gen.Corners.size());
		indexOf.reserve(vertices.size());

		static_assert(sizeof(Vertex) == 12 * sizeof(f32), "Padding would break comparing vertices by bytes");
		for (const Vertex& corner : gen.Corners)
		{
			const std::string_view bytes(reinterpret_cast<const char*>(&corner), sizeof(Vertex));
			const auto [it, isNew] = indexOf.try_emplace(bytes, static_cast<u32>(welded.size()));
			if (isNew)
			{
				welded.push_back(corner);
			}
			weldedIndices.push_back(it->second);
		}

		vertices = std::move(welded);
		indices  = std::move(weldedIndices);
		return true;
	}

	bool TangentGenerator::GenerateForAsset(const std::string& assetPath)
	{
		ZoneScoped;

		const Result<std::shared_ptr<const PreparedMesh>> loaded = AssetFile::Load(assetPath).and_then(&PreparedMesh::Read);
		if (!loaded)
		{
			KOI_LOG(Import, Error, "Couldn't generate tangents for {}: {}", assetPath, loaded.error().Message);
			return false;
		}
		const PreparedMesh& mesh = **loaded;

		std::vector<Vertex>      vertices;
		std::vector<u32>         indices;
		std::vector<MeshSubmesh> submeshes;
		u32 generated = 0;

		for (MeshSubmesh sub : mesh.Submeshes)
		{
			// The file only stores where a submesh's vertices start, so its highest index says where they end
			const std::span<const u32> subIndices = mesh.Indices.subspan(sub.IndexOffset, sub.IndexCount);
			const size_t vertexCount = subIndices.empty() ? 0 : static_cast<size_t>(std::ranges::max(subIndices)) + 1;
			if (sub.VertexOffset + vertexCount > mesh.Vertices.size())
			{
				KOI_LOG(Import, Error, "Couldn't generate tangents for {}: an index points outside its vertices", assetPath);
				return false;
			}

			const std::span<const Vertex> subVertices = mesh.Vertices.subspan(sub.VertexOffset, vertexCount);
			std::vector<Vertex> newVertices(subVertices.begin(), subVertices.end());
			std::vector<u32>    newIndices(subIndices.begin(), subIndices.end());

			if (std::ranges::any_of(newVertices, [](const Vertex& v) { return v.Tangent.w == 0.0f; }))
			{
				if (newIndices.size() % 3 != 0 || !Generate(newVertices, newIndices))
				{
					KOI_LOG(Import, Error, "Couldn't generate tangents for {}, so it was left as it is", assetPath);
					return false;
				}
				++generated;
			}

			sub.VertexOffset = static_cast<i32>(vertices.size());
			sub.IndexOffset  = static_cast<u32>(indices.size());
			sub.IndexCount   = static_cast<u32>(newIndices.size());
			submeshes.push_back(sub);
			vertices.insert(vertices.end(), newVertices.begin(), newVertices.end());
			indices.insert(indices.end(), newIndices.begin(), newIndices.end());
		}

		if (generated == 0)
		{
			KOI_LOG(Import, Info, "{} already has tangents", assetPath);
			return true;
		}

		MeshHeader header  = *mesh.Header;
		header.VertexCount = static_cast<u32>(vertices.size());
		header.IndexCount  = static_cast<u32>(indices.size());

		// Same file and id, only the geometry chunks are replaced
		AssetFileWriter writer(AssetFileType::Mesh, mesh.File.Id());
		writer.SetSourceHash(mesh.File.Header().SourceHash);
		writer.SetImporterVersion(mesh.File.Header().ImporterVersion);
		for (const ChunkEntry& chunk : mesh.File.Chunks())
		{
			const EnumFlag<ChunkFlags> flags(static_cast<ChunkFlags>(chunk.Flags));
			switch (chunk.Type)
			{
			case MeshChunk::Header:    writer.AddChunkValue(chunk.Type, chunk.Version, header, flags); break;
			case MeshChunk::Vertices:  writer.AddChunkArray<Vertex>(chunk.Type, chunk.Version, vertices, flags); break;
			case MeshChunk::Indices:   writer.AddChunkArray<u32>(chunk.Type, chunk.Version, indices, flags); break;
			case MeshChunk::Submeshes: writer.AddChunkArray<MeshSubmesh>(chunk.Type, chunk.Version, submeshes, flags); break;
			default:                   writer.AddChunk(chunk.Type, chunk.Version, mesh.File.ChunkBytes(chunk), flags); break;
			}
		}

		const fs::path file = VirtualFileSystem::Get().TryResolveToDisk(assetPath).value_or(fs::path());
		if (file.empty() || !writer.Save(file))
		{
			KOI_LOG(Import, Error, "Couldn't save {} with its new tangents", assetPath);
			return false;
		}

		KOI_LOG(Import, Info, "Generated tangents for {} submesh(es) of {} ({} -> {} vertices)",
			generated, assetPath, mesh.Vertices.size(), vertices.size());

		// The old buffers are destroyed once the GPU is done with them, so swapping in place is safe
		AssetCache<GPUMesh>& cache = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();
		const AssetKey key(assetPath);
		if (cache.IsResident(key))
		{
			cache.Reload(Engine::Get().GetSubsystem<Renderer>().GetDevice(), key);
		}
		return true;
	}
}
