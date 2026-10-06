#pragma once
#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Graphics/Types.h"

namespace koi
{
	namespace MeshChunk
	{
		inline constexpr u32 Header         = FourCC("MSHD");  // MeshHeader
		inline constexpr u32 Vertices       = FourCC("VERT");  // Vertex[], in the layout MeshHeader::Layout names
		inline constexpr u32 Indices        = FourCC("INDX");  // u32[]
		inline constexpr u32 Submeshes      = FourCC("SUBM");  // MeshSubmesh[]
		inline constexpr u32 MaterialSlots  = FourCC("MSLT");  // MeshMaterialSlot[]
		inline constexpr u32 Strings        = FourCC("STRS");  // UTF-8 names, referenced by offset
		inline constexpr u32 ImportSettings = FourCC("IMPS");  // editor only
		inline constexpr u32 SourcePath     = FourCC("SRCP");  // editor only
	}

	// Which vertex stream(s) the file carries. Split position/attribute/skin streams get their own values later
	enum class VertexLayout : u32
	{
		Interleaved = 1,  // koi::Vertex: position, normal, tangent, uv (48 bytes)
	};

	struct MeshHeader
	{
		VertexLayout Layout            = VertexLayout::Interleaved;
		u32          VertexCount       = 0;
		u32          IndexCount        = 0;
		u32          SubmeshCount      = 0;
		u32          MaterialSlotCount = 0;
		u32          Flags             = 0;
		f32          BoundsMin[3]      = {};
		f32          BoundsMax[3]      = {};
	};
	static_assert(sizeof(MeshHeader) == 48 && std::is_trivially_copyable_v<MeshHeader>);

	struct MeshSubmesh
	{
		u32 IndexOffset  = 0;
		u32 IndexCount   = 0;
		i32 VertexOffset = 0;
		u32 MaterialSlot = 0;  // MaterialSlotCount means "no material": the renderer's fallback
		f32 BoundsMin[3] = {};
		f32 BoundsMax[3] = {};
	};
	static_assert(sizeof(MeshSubmesh) == 40 && std::is_trivially_copyable_v<MeshSubmesh>);

	struct MeshMaterialSlot
	{
		std::array<byte, 16> Material   = {};  // default .kmat for this slot, nil for none
		u32                  NameOffset = 0;   // into the Strings chunk
		u32                  NameLength = 0;
	};
	static_assert(sizeof(MeshMaterialSlot) == 24 && std::is_trivially_copyable_v<MeshMaterialSlot>);

	static_assert(sizeof(Vertex) == 48 && std::is_trivially_copyable_v<Vertex>, "VertexLayout::Interleaved assumes this exact Vertex");

	// A .kmesh held in memory, with views into its chunks
	struct PreparedMesh
	{
		AssetFile                         File;
		const MeshHeader*                 Header = nullptr;
		std::span<const Vertex>           Vertices;
		std::span<const u32>              Indices;
		std::span<const MeshSubmesh>      Submeshes;
		std::span<const MeshMaterialSlot> Slots;
		std::string_view                  Strings;

		[[nodiscard]] std::string_view SlotName(const MeshMaterialSlot& slot) const;
	};

	[[nodiscard]] std::shared_ptr<const PreparedMesh> ReadPreparedMesh(AssetFile file, std::string_view debugName);
}
