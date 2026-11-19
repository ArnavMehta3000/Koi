#include "Koi/Engine/Assets/AssetIndex.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <chrono>
#include <toml++/toml.hpp>

namespace koi
{
	namespace
	{
		// Prefabs are text, so the id is a top-level key rather than a header field
		Result<UUID> ReadPrefabId(const fs::path& diskPath)
		{
			try
			{
				const toml::table file = toml::parse_file(diskPath.string());
				return utils::UUIDFromString(file["id"].value_or(std::string()));
			}
			catch (const toml::parse_error& e)
			{
				return Error::Make("line {}: {}", e.source().begin.line, e.description());
			}
		}
	}

	void AssetIndex::Rebuild()
	{
		ZoneScoped;
		const auto start = std::chrono::steady_clock::now();

		Clear();
		VirtualFileSystem::Get().ForEachDiskRoot([this](std::string_view scheme, const fs::path& root)
		{
			ScanRoot(scheme, root);
		});

		const f64 ms = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - start).count();
		KOI_LOG(Assets, Info, "Indexed {} asset(s) in {:.1f} ms", Count(), ms);
	}

	void AssetIndex::Clear()
	{
		m_entries.clear();
		m_idsByPath.clear();
	}

	void AssetIndex::ScanRoot(std::string_view scheme, const fs::path& root)
	{
		ZoneScoped;

		const fs::path contentRoot = root / "Content";
		std::error_code ec;
		if (!fs::is_directory(contentRoot, ec)) return;

		for (const fs::directory_entry& file : fs::recursive_directory_iterator(contentRoot, ec))
		{
			if (!file.is_regular_file()) continue;

			if (const AssetTypeInfo* kind = utils::FindAssetTypeForFile(file.path().filename().string()); kind && kind->Kind == AssetKind::Prefab)
			{
				const Result<UUID> id = ReadPrefabId(file.path());
				if (!id)
				{
					KOI_LOG(Assets, Error, "Skipped {}: {}", file.path().string(), id.error().Message);
					continue;
				}

				const fs::path relative = fs::relative(file.path(), root, ec);
				if (!ec) Add(*id, AssetFileType::Prefab, std::string(scheme) + "://" + relative.generic_string());
				continue;
			}

			const AssetFileType expected = AssetFile::TypeForPath(file.path());
			if (expected == AssetFileType::Unknown) continue;

			const Result<AssetFileHeader> header = AssetFile::ReadHeader(file.path());
			if (!header)
			{
				KOI_LOG(Assets, Error, "Skipped {}: {}", file.path().string(), header.error().Message);
				continue;
			}

			const fs::path relative = fs::relative(file.path(), root, ec);
			if (ec) continue;

			std::string virtualPath = std::string(scheme) + "://" + relative.generic_string();
			if (header->Type != expected)
			{
				KOI_LOG(Assets, Warn, "{} is named like a {} but holds a {}. Going by what's inside",
					virtualPath, AssetFile::ToString(expected), AssetFile::ToString(header->Type));
			}

			Add(UUID(header->Id), header->Type, std::move(virtualPath));
		}

		if (ec)
		{
			KOI_LOG(Assets, Warn, "Stopped scanning {} early: {}", contentRoot.string(), ec.message());
		}
	}

	bool AssetIndex::Add(const UUID& id, AssetFileType type, std::string virtualPath)
	{
		if (id.is_nil())
		{
			KOI_LOG(Assets, Warn, "{} has no id, so nothing can reference it. Reimport it to give it one", virtualPath);
			return false;
		}

		if (const auto it = m_entries.find(id); it != m_entries.end() && it->second.VirtualPath != virtualPath)
		{
			// Usually a file copied in Explorer, which copies its id along with it
			KOI_LOG(Assets, Warn, "{} and {} share the id {}. Keeping the first, the copy needs a fresh id before it can be used",
				it->second.VirtualPath, virtualPath, utils::ToString(id));
			return false;
		}

		if (const auto old = m_idsByPath.find(virtualPath); old != m_idsByPath.end() && old->second != id)
			m_entries.erase(old->second);  // this file was re-saved under a new id

		m_idsByPath[virtualPath] = id;
		m_entries[id] = AssetIndexEntry{ .Id = id, .Type = type, .VirtualPath = std::move(virtualPath) };
		return true;
	}

	const AssetIndexEntry* AssetIndex::Find(const UUID& id) const
	{
		const auto it = m_entries.find(id);
		return it != m_entries.end() ? &it->second : nullptr;
	}

	const AssetIndexEntry* AssetIndex::FindByPath(std::string_view virtualPath) const
	{
		const auto it = m_idsByPath.find(std::string(virtualPath));
		return it != m_idsByPath.end() ? Find(it->second) : nullptr;
	}
}
