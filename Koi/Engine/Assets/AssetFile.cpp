#include "Koi/Engine/Assets/AssetFile.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>

namespace koi
{
	namespace
	{
		struct FileTypeInfo
		{
			AssetFileType    Type;
			std::string_view Name;
			std::string_view Extension;
		};

		constexpr FileTypeInfo kFileTypes[] =
		{
			{ AssetFileType::Mesh,      "mesh",      ".kmesh" },
			{ AssetFileType::Texture,   "texture",   ".ktex"  },
			{ AssetFileType::Material,  "material",  ".kmat"  },
			{ AssetFileType::Skeleton,  "skeleton",  ".kskel" },
			{ AssetFileType::Animation, "animation", ".kanim" },
		};

		const FileTypeInfo* FindFileType(AssetFileType type)
		{
			const auto it = std::ranges::find(kFileTypes, type, &FileTypeInfo::Type);
			return it != std::end(kFileTypes) ? &*it : nullptr;
		}

		constexpr u64 AlignUp(u64 value, u64 alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		bool CheckHeader(const AssetFileHeader& header, std::string_view debugName)
		{
			if (header.Magic != kAssetFileMagic)
			{
				KOI_LOG(Assets, Error, "{} isn't a Koi asset file (bad magic)", debugName);
				return false;
			}

			if (header.FormatVersion > kAssetFormatVersion)
			{
				KOI_LOG(Assets, Error, "{} was written by a newer Koi (format v{}), this build only understands up to v{}",
					debugName, header.FormatVersion, kAssetFormatVersion);
				return false;
			}

			if (header.FormatVersion < kAssetFormatVersion)
			{
				KOI_LOG(Assets, Error, "{} uses an old asset format (v{}, current is v{}). Reimport it from its source",
					debugName, header.FormatVersion, kAssetFormatVersion);
				return false;
			}

			if (!FindFileType(header.Type))
			{
				KOI_LOG(Assets, Error, "{} claims to be asset type {}, which Koi doesn't know about", debugName, static_cast<u16>(header.Type));
				return false;
			}

			return true;
		}
	}

	std::string utils::FourCCToString(u32 code)
	{
		std::string text(4, '?');
		for (size_t i = 0; i < 4; ++i)
		{
			const char c = static_cast<char>((code >> (i * 8)) & 0xFF);
			if (c >= 0x20 && c < 0x7F) text[i] = c;
		}
		return text;
	}

	std::string_view AssetFile::ToString(AssetFileType type)
	{
		const FileTypeInfo* info = FindFileType(type);
		return info ? info->Name : "unknown";
	}

	std::string_view AssetFile::ExtensionFor(AssetFileType type)
	{
		const FileTypeInfo* info = FindFileType(type);
		return info ? info->Extension : std::string_view();
	}

	AssetFileType AssetFile::TypeForPath(const fs::path& path)
	{
		std::string extension = path.extension().string();
		std::ranges::transform(extension, extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		const auto it = std::ranges::find(kFileTypes, std::string_view(extension), &FileTypeInfo::Extension);
		return it != std::end(kFileTypes) ? it->Type : AssetFileType::Unknown;
	}

	std::optional<AssetFileHeader> AssetFile::ReadHeader(const fs::path& diskPath)
	{
		std::ifstream file(diskPath, std::ios::binary);
		if (!file) return std::nullopt;

		AssetFileHeader header;
		if (!file.read(reinterpret_cast<char*>(&header), sizeof(header)))
		{
			KOI_LOG(Assets, Warn, "{} is too short to be a Koi asset", diskPath.string());
			return std::nullopt;
		}

		if (!CheckHeader(header, diskPath.string())) return std::nullopt;
		return header;
	}

	std::optional<AssetFile> AssetFile::FromBytes(std::vector<byte> bytes, std::string_view debugName)
	{
		ZoneScoped;

		if (bytes.size() < sizeof(AssetFileHeader))
		{
			KOI_LOG(Assets, Error, "{} is only {} bytes, too short to be a Koi asset", debugName, bytes.size());
			return std::nullopt;
		}

		AssetFile file(std::move(bytes));
		const AssetFileHeader& header = file.Header();
		if (!CheckHeader(header, debugName)) return std::nullopt;

		const u64 fileSize   = file.m_bytes.size();
		const u64 tableBytes = static_cast<u64>(header.ChunkCount) * sizeof(ChunkEntry);
		if (header.ChunkTableOffset % alignof(ChunkEntry) != 0 || header.ChunkTableOffset > fileSize || tableBytes > fileSize - header.ChunkTableOffset)
		{
			KOI_LOG(Assets, Error, "{} has a chunk table that runs off the end of the file. It's probably truncated or corrupt", debugName);
			return std::nullopt;
		}

		for (const ChunkEntry& chunk : file.Chunks())
		{
			if (chunk.Offset % kAssetChunkAlignment != 0 || chunk.Offset > fileSize || chunk.Size > fileSize - chunk.Offset)
			{
				KOI_LOG(Assets, Error, "{} has a broken '{}' chunk (offset {}, size {}, file is {} bytes)",
					debugName, utils::FourCCToString(chunk.Type), chunk.Offset, chunk.Size, fileSize);
				return std::nullopt;
			}
		}

		KOI_LOG(Assets, Trace, "Opened {} {} with {} chunk(s)", ToString(header.Type), debugName, header.ChunkCount);
		return file;
	}

	std::optional<AssetFile> AssetFile::Load(std::string_view virtualPath)
	{
		ZoneScoped;
		ZoneText(virtualPath.data(), virtualPath.size());

		std::optional<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(virtualPath);
		if (!bytes)
		{
			KOI_LOG(Assets, Error, "Couldn't read {}", virtualPath);
			return std::nullopt;
		}

		return FromBytes(std::move(*bytes), virtualPath);
	}

	UUID AssetFile::Id() const
	{
		return UUID(Header().Id);
	}

	std::span<const ChunkEntry> AssetFile::Chunks() const
	{
		const AssetFileHeader& header = Header();
		return { reinterpret_cast<const ChunkEntry*>(m_bytes.data() + header.ChunkTableOffset), header.ChunkCount };
	}

	const ChunkEntry* AssetFile::FindChunk(u32 type) const
	{
		const std::span<const ChunkEntry> chunks = Chunks();
		const auto it = std::ranges::find(chunks, type, &ChunkEntry::Type);
		return it != chunks.end() ? &*it : nullptr;
	}

	std::span<const byte> AssetFile::ChunkBytes(const ChunkEntry& chunk) const
	{
		return { m_bytes.data() + chunk.Offset, static_cast<size_t>(chunk.Size) };
	}

	std::span<const byte> AssetFile::ChunkBytes(u32 type) const
	{
		const ChunkEntry* chunk = FindChunk(type);
		return chunk ? ChunkBytes(*chunk) : std::span<const byte>();
	}

	std::vector<byte> AssetFile::Cooked() const
	{
		AssetFileWriter writer(Type(), Id());
		writer.SetSourceHash(Header().SourceHash);
		writer.SetImporterVersion(Header().ImporterVersion);
		for (const ChunkEntry& chunk : Chunks())
			writer.AddChunk(chunk.Type, chunk.Version, ChunkBytes(chunk), EnumFlag<ChunkFlags>(chunk.Flags));
		return writer.Build(true);
	}

	AssetFileWriter::AssetFileWriter(AssetFileType type, const UUID& id)
	{
		m_header.Type = type;
		const auto idBytes = id.as_bytes();
		std::memcpy(m_header.Id.data(), idBytes.data(), m_header.Id.size());
	}

	void AssetFileWriter::AddChunk(u32 type, u16 version, std::span<const byte> data, EnumFlag<ChunkFlags> flags)
	{
		const bool duplicate = std::ranges::any_of(m_chunks, [type](const PendingChunk& c) { return c.Entry.Type == type; });
		if (duplicate)
		{
			KOI_LOG(Assets, Warn, "Asset already has a '{}' chunk, adding another one anyway. Readers will only ever see the first", utils::FourCCToString(type));
		}

		m_chunks.push_back(PendingChunk
		{
			.Entry = { .Type = type, .Version = version, .Flags = static_cast<u16>(flags.Raw()), .Size = data.size() },
			.Data  = { data.begin(), data.end() },
		});
	}

	std::vector<byte> AssetFileWriter::Build(bool stripEditorOnly) const
	{
		ZoneScoped;

		std::vector<const PendingChunk*> kept;
		for (const PendingChunk& chunk : m_chunks)
		{
			if (stripEditorOnly && EnumFlag<ChunkFlags>(chunk.Entry.Flags).Has(ChunkFlags::EditorOnly)) continue;
			kept.push_back(&chunk);
		}

		AssetFileHeader header  = m_header;
		header.ChunkCount       = static_cast<u32>(kept.size());
		header.ChunkTableOffset = sizeof(AssetFileHeader);
		if (stripEditorOnly) header.Flags |= static_cast<u32>(AssetFileFlags::Cooked);

		std::vector<ChunkEntry> table;
		table.reserve(kept.size());

		u64 cursor = AlignUp(header.ChunkTableOffset + kept.size() * sizeof(ChunkEntry), kAssetChunkAlignment);
		for (const PendingChunk* chunk : kept)
		{
			ChunkEntry entry = chunk->Entry;
			entry.Offset     = cursor;
			table.push_back(entry);
			cursor = AlignUp(cursor + entry.Size, kAssetChunkAlignment);
		}

		std::vector<byte> bytes(cursor, 0);  // zeroed, so padding is deterministic
		std::memcpy(bytes.data(), &header, sizeof(header));
		if (!table.empty()) std::memcpy(bytes.data() + header.ChunkTableOffset, table.data(), table.size() * sizeof(ChunkEntry));

		for (size_t i = 0; i < kept.size(); ++i)
		{
			if (!kept[i]->Data.empty())
			{
				std::memcpy(bytes.data() + table[i].Offset, kept[i]->Data.data(), kept[i]->Data.size());
			}
		}

		return bytes;
	}

	bool AssetFileWriter::Save(const fs::path& diskPath, bool stripEditorOnly) const
	{
		ZoneScoped;

		const std::vector<byte> bytes = Build(stripEditorOnly);

		std::error_code ec;
		fs::create_directories(diskPath.parent_path(), ec);

		fs::path tempPath = diskPath;
		tempPath += ".tmp";

		{
			std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
			if (!out || !out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
			{
				KOI_LOG(Assets, Error, "Couldn't write {}. Is the folder read-only, or the file open somewhere else?", tempPath.string());
				fs::remove(tempPath, ec);
				return false;
			}
		}

		fs::rename(tempPath, diskPath, ec);
		if (ec)
		{
			KOI_LOG(Assets, Error, "Wrote {} but couldn't move it into place: {}", tempPath.string(), ec.message());
			fs::remove(tempPath, ec);
			return false;
		}

		KOI_LOG(Assets, Info, "Saved {} {} ({} chunk(s), {:.1f} KB)",
			AssetFile::ToString(m_header.Type), diskPath.filename().string(), m_chunks.size(), static_cast<f64>(bytes.size()) / 1024.0);
		return true;
	}
}
