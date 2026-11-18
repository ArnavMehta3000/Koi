#pragma once
#include "Koi/Engine/Core/EnumFlags.h"
#include "Koi/Engine/Core/Error.h"
#include "Koi/Engine/Core/Macros.h"
#include "Koi/Engine/Core/Types.h"
#include "Koi/Engine/Core/UUID.h"
#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace koi
{
	namespace fs = std::filesystem;

	namespace utils
	{
		consteval u32 FourCC(const char (&code)[5])
		{
			return  static_cast<u32>(static_cast<u8>(code[0]))
				| (static_cast<u32>(static_cast<u8>(code[1])) << 8)
				| (static_cast<u32>(static_cast<u8>(code[2])) << 16)
				| (static_cast<u32>(static_cast<u8>(code[3])) << 24);
		}

		[[nodiscard]] std::string FourCCToString(u32 code);
	}

	inline constexpr u32 kAssetFileMagic      = utils::FourCC("KOIA");
	inline constexpr u16 kAssetFormatVersion  = 1;
	inline constexpr u64 kAssetChunkAlignment = 16;

	enum class AssetFileType : u16
	{
		Unknown = 0,
		Mesh,
		Texture,
		Material,
		Skeleton,
		Animation,
		Prefab,  // text, indexed from its id line
	};

	enum class AssetFileFlags : u32
	{
		None   = 0,
		Cooked = KOI_BIT(0),  // editor-only chunks were stripped
	};

	enum class ChunkFlags : u16
	{
		None       = 0,
		EditorOnly = KOI_BIT(0),  // import settings, source path, thumbnail; dropped by the cook
	};

	// On-disk layout, parsed as raw bytes
	struct AssetFileHeader
	{
		u32                  Magic            = kAssetFileMagic;
		u16                  FormatVersion    = kAssetFormatVersion;
		AssetFileType        Type             = AssetFileType::Unknown;
		std::array<byte, 16> Id               = {};
		u64                  SourceHash       = 0;  // of the imported source file, to spot stale imports
		u32                  ImporterVersion  = 0;
		u32                  ChunkCount       = 0;
		u64                  ChunkTableOffset = 0;
		u32                  Flags            = 0;  // AssetFileFlags
		u32                  Reserved[3]      = {};
	};

	struct ChunkEntry
	{
		u32 Type    = 0;  // Chunk Type FourCC
		u16 Version = 0;
		u16 Flags   = 0;  // ChunkFlags
		u64 Offset  = 0;  // from the start of the file, kAssetChunkAlignment aligned
		u64 Size    = 0;
	};

	static_assert(sizeof(AssetFileHeader) == 64 && std::is_trivially_copyable_v<AssetFileHeader>);
	static_assert(offsetof(AssetFileHeader, Id) == 8 && offsetof(AssetFileHeader, ChunkTableOffset) == 40);
	static_assert(sizeof(ChunkEntry) == 24 && std::is_trivially_copyable_v<ChunkEntry>);
	static_assert(offsetof(ChunkEntry, Offset) == 8);

	// A whole asset file in memory
	class AssetFile
	{
	public:
		[[nodiscard]] static Result<AssetFile> FromBytes(std::vector<byte> bytes);
		[[nodiscard]] static Result<AssetFile> Load(std::string_view virtualPath);

		// Read 64 byte header, cheap for scanning
		[[nodiscard]] static Result<AssetFileHeader> ReadHeader(const fs::path& diskPath);

		[[nodiscard]] static std::string_view ToString(AssetFileType type);
		[[nodiscard]] static std::string_view ExtensionFor(AssetFileType type);     // ".kmesh"
		[[nodiscard]] static AssetFileType    TypeForPath(const fs::path& path);    // Unknown if not a binary asset

		[[nodiscard]] const AssetFileHeader&  Header() const { return *reinterpret_cast<const AssetFileHeader*>(m_bytes.data()); }
		[[nodiscard]] AssetFileType           Type()   const { return Header().Type; }
		[[nodiscard]] UUID                    Id()     const;
		[[nodiscard]] std::span<const ChunkEntry> Chunks() const;

		// Same assset without its EditorOnly chunks (shipped game content)
		[[nodiscard]] std::vector<byte> Cooked() const;

		[[nodiscard]] const ChunkEntry*     FindChunk(u32 type) const;
		[[nodiscard]] std::span<const byte> ChunkBytes(const ChunkEntry& chunk) const;
		[[nodiscard]] std::span<const byte> ChunkBytes(u32 type) const;

		// Null when the chunk is missing or too small for a T
		template<typename T>
		[[nodiscard]] const T* ChunkAs(u32 type) const
		{
			static_assert(std::is_trivially_copyable_v<T> && alignof(T) <= kAssetChunkAlignment);

			const std::span<const byte> bytes = ChunkBytes(type);
			return bytes.size() >= sizeof(T) ? reinterpret_cast<const T*>(bytes.data()) : nullptr;
		}

		// Empty when the chunk is missing or not a whole number of Ts
		template<typename T>
		[[nodiscard]] std::span<const T> ChunkArray(u32 type) const
		{
			static_assert(std::is_trivially_copyable_v<T> && alignof(T) <= kAssetChunkAlignment);

			const std::span<const byte> bytes = ChunkBytes(type);
			if (bytes.size() % sizeof(T) != 0) return {};
			return { reinterpret_cast<const T*>(bytes.data()), bytes.size() / sizeof(T) };
		}

	private:
		explicit AssetFile(std::vector<byte> bytes) : m_bytes(std::move(bytes)) {}

	private:
		std::vector<byte> m_bytes;
	};

	// Collects chunks, then lays them out as header, chunk table, aligned chunk data
	class AssetFileWriter
	{
	public:
		AssetFileWriter(AssetFileType type, const UUID& id);

		void SetSourceHash(u64 hash)          { m_header.SourceHash = hash; }
		void SetImporterVersion(u32 version)  { m_header.ImporterVersion = version; }

		void AddChunk(u32 type, u16 version, std::span<const byte> data, EnumFlag<ChunkFlags> flags = {});

		template<typename T>
		void AddChunkArray(u32 type, u16 version, std::span<const T> items, EnumFlag<ChunkFlags> flags = {})
		{
			static_assert(std::is_trivially_copyable_v<T>);
			AddChunk(type, version, { reinterpret_cast<const byte*>(items.data()), items.size_bytes() }, flags);
		}

		template<typename T>
		void AddChunkValue(u32 type, u16 version, const T& value, EnumFlag<ChunkFlags> flags = {})
		{
			AddChunkArray(type, version, std::span<const T>(&value, 1), flags);
		}

		[[nodiscard]] std::vector<byte> Build(bool stripEditorOnly = false) const;

		// Writes beside the target and renames over it, so a crash never leaves half a file
		bool Save(const fs::path& diskPath, bool stripEditorOnly = false) const;

	private:
		struct PendingChunk
		{
			ChunkEntry        Entry;
			std::vector<byte> Data;
		};

		AssetFileHeader           m_header;
		std::vector<PendingChunk> m_chunks;
	};
}
