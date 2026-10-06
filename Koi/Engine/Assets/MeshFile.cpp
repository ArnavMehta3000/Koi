#include "Koi/Engine/Assets/MeshFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	std::string_view PreparedMesh::SlotName(const MeshMaterialSlot& slot) const
	{
		if (static_cast<u64>(slot.NameOffset) + slot.NameLength > Strings.size()) return {};
		return Strings.substr(slot.NameOffset, slot.NameLength);
	}

	std::shared_ptr<const PreparedMesh> PreparedMesh::Read(AssetFile file, std::string_view debugName)
	{
		ZoneScoped;

		if (file.Type() != AssetFileType::Mesh)
		{
			KOI_LOG_ERROR("{} isn't a mesh, it holds a {}", debugName, AssetFile::ToString(file.Type()));
			return nullptr;
		}

		auto prepared = std::make_shared<PreparedMesh>(PreparedMesh{ .File = std::move(file) });
		const AssetFile& f = prepared->File;

		prepared->Header    = f.ChunkAs<MeshHeader>(MeshChunk::Header);
		prepared->Vertices  = f.ChunkArray<Vertex>(MeshChunk::Vertices);
		prepared->Indices   = f.ChunkArray<u32>(MeshChunk::Indices);
		prepared->Submeshes = f.ChunkArray<MeshSubmesh>(MeshChunk::Submeshes);
		prepared->Slots     = f.ChunkArray<MeshMaterialSlot>(MeshChunk::MaterialSlots);

		const std::span<const byte> strings = f.ChunkBytes(MeshChunk::Strings);
		prepared->Strings = std::string_view(reinterpret_cast<const char*>(strings.data()), strings.size());

		const MeshHeader* header = prepared->Header;
		if (!header || header->Layout != VertexLayout::Interleaved)
		{
			KOI_LOG_ERROR("{} has no mesh header, or a vertex layout this build can't read. Reimport it", debugName);
			return nullptr;
		}

		if (prepared->Vertices.size() != header->VertexCount
			|| prepared->Indices.size() != header->IndexCount
			|| prepared->Submeshes.size() != header->SubmeshCount
			|| prepared->Slots.size() != header->MaterialSlotCount
			|| prepared->Vertices.empty()
			|| prepared->Indices.empty()
			|| prepared->Submeshes.empty())
		{
			KOI_LOG_ERROR("{} says it has {} vertices, {} indices and {} submesh(es), but the data doesn't match. Reimport it",
				debugName, header->VertexCount, header->IndexCount, header->SubmeshCount);
			return nullptr;
		}

		for (const MeshSubmesh& sub : prepared->Submeshes)
		{
			if (static_cast<u64>(sub.IndexOffset) + sub.IndexCount > header->IndexCount
				|| sub.VertexOffset < 0
				|| static_cast<u32>(sub.VertexOffset) >= header->VertexCount
				|| sub.MaterialSlot > header->MaterialSlotCount)
			{
				KOI_LOG_ERROR("{} has a submesh pointing outside its own data. Reimport it", debugName);
				return nullptr;
			}
		}

		return prepared;
	}
}
