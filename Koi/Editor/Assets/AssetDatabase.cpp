#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Engine/Assets/AssetIdentity.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <fstream>

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kManifestName = "AssetRegistry.toml";
		constexpr std::string_view kCacheDir     = ".koi";
		constexpr std::string_view kCacheName    = "AssetIndex.toml";

		// FNV-1a. Only ever compared against another hash from this same function, so the
		// choice matters less than it being stable across runs
		u64 HashBytes(std::span<const char> bytes)
		{
			u64 hash = 14695981039346656037ull;
			for (const char b : bytes)
			{
				hash ^= static_cast<u8>(b);
				hash *= 1099511628211ull;
			}
			return hash;
		}

		i64 ModTimeOf(const fs::directory_entry& entry)
		{
			std::error_code ec;
			const fs::file_time_type time = entry.last_write_time(ec);
			return ec ? 0 : time.time_since_epoch().count();
		}

		std::string ParentFolder(std::string_view virtualPath)
		{
			const size_t slash = virtualPath.find_last_of('/');
			return slash == std::string_view::npos
				? std::string(virtualPath)
				: std::string(virtualPath.substr(0, slash));
		}
	}

	AssetDatabase::AssetDatabase(fs::path engineSource, fs::path projectSource)
		: m_engineSource(std::move(engineSource))
		, m_projectSource(std::move(projectSource))
		, m_manifestPath(m_projectSource / kManifestName)
		, m_cachePath(m_projectSource / kCacheDir / kCacheName)
	{
	}

	void AssetDatabase::Refresh()
	{
		m_entries.clear();
		m_folders.clear();
		m_manifest.clear();
		m_cache.clear();
		m_manifestDirty = false;
		m_cacheDirty    = false;

		LoadManifest();
		LoadCache();

		ScanRoot(m_engineSource,  "engine",  "Content");
		ScanRoot(m_projectSource, "project", "Assets");

		// Anything the manifest still knows about that no scan claimed has lost its file.
		// Kept rather than dropped - the UUID is what a scene may still reference
		for (const auto& [relative, record] : m_manifest)
		{
			if (record.Claimed) continue;

			const AssetTypeInfo* type = FindAssetTypeByName(record.Type);
			const std::string virtualPath = "project://" + relative;

			m_entries.push_back(AssetEntry
			{
				.Id          = record.Id,
				.VirtualPath = virtualPath,
				.Name        = fs::path(relative).filename().string(),
				.Folder      = ParentFolder(virtualPath),
				.Kind        = type ? type->Kind : AssetKind::Unknown,
				.State       = AssetState::Missing,
				.Settings    = record.Settings,
			});

			KOI_LOG_WARN("Asset missing on disk: {} ({})", relative, ToString(record.Id));
		}

		std::ranges::sort(m_entries, {}, &AssetEntry::VirtualPath);

		for (const AssetEntry& entry : m_entries) m_folders.push_back(entry.Folder);
		std::ranges::sort(m_folders);
		m_folders.erase(std::ranges::unique(m_folders).begin(), m_folders.end());

		if (m_manifestDirty) SaveManifest();
		if (m_cacheDirty)    SaveCache();

		// The engine resolves scene references through this; the editor is its populator
		AssetIdentity& identity = AssetIdentity::Get();
		identity.Clear();
		for (const AssetEntry& entry : m_entries) identity.Set(entry.Id, entry.VirtualPath);

		KOI_LOG_INFO("Asset database: {} asset(s), {} missing", m_entries.size(), MissingCount());
	}

	void AssetDatabase::ScanRoot(const fs::path& root, std::string_view scheme, std::string_view contentDir)
	{
		const fs::path contentRoot = root / contentDir;
		if (!fs::exists(contentRoot)) return;

		const bool identified = (scheme == "project");  // engine content gets no UUID

		std::error_code ec;
		for (const fs::directory_entry& file : fs::recursive_directory_iterator(contentRoot, ec))
		{
			if (!file.is_regular_file()) continue;

			const AssetTypeInfo* type = FindAssetType(file.path().extension().string());
			if (!type) continue;  // .h/.cpp/.bin and anything else unregistered

			const std::string relative = fs::relative(file.path(), root, ec).generic_string();
			if (ec) continue;

			AssetEntry entry
			{
				.VirtualPath = std::string(scheme) + "://" + relative,
				.Name        = file.path().filename().string(),
				.Kind        = type->Kind,
				.State       = AssetState::Unloaded,
				.DiskPath    = file.path(),
			};
			entry.Folder = ParentFolder(entry.VirtualPath);

			if (identified) ReconcileEntry(entry, relative, *type, file);

			m_entries.push_back(std::move(entry));
		}
	}

	void AssetDatabase::ReconcileEntry(AssetEntry& entry, const std::string& relative,
	                                   const AssetTypeInfo& type, const fs::directory_entry& file)
	{
		const CacheRecord* cached = nullptr;
		if (const auto it = m_cache.find(relative); it != m_cache.end()) cached = &it->second;

		if (const auto it = m_manifest.find(relative); it != m_manifest.end())
		{
			it->second.Claimed = true;
			entry.Id           = it->second.Id;
			entry.Settings     = it->second.Settings;

			if (it->second.Type != type.Name)
			{
				it->second.Type = std::string(type.Name);
				m_manifestDirty = true;
			}
		}
		else
		{
			if (!TryRepairRename(entry, HashFile(file.path(), cached)))
			{
				entry.Id = NewUUID();
				KOI_LOG_DEBUG("New asset {} -> {}", relative, ToString(entry.Id));
			}

			m_manifest.emplace(relative, ManifestRecord
			{
				.Id       = entry.Id,
				.Type     = std::string(type.Name),
				.Settings = entry.Settings,
				.Claimed  = true,
			});
			m_manifestDirty = true;
		}

		// Refresh the derived record whenever the file looks touched
		std::error_code ec;
		const u64 size    = file.file_size(ec);
		const i64 modTime = ModTimeOf(file);
		if (!cached || cached->Size != size || cached->ModTime != modTime)
		{
			m_cache[relative] = CacheRecord
			{
				.Size    = size,
				.ModTime = modTime,
				.Hash    = HashFile(file.path(), nullptr),
			};
			m_cacheDirty = true;
		}
	}

	bool AssetDatabase::TryRepairRename(AssetEntry& entry, u64 hash)
	{
		if (hash == 0) return false;

		for (auto& [oldRelative, record] : m_manifest)
		{
			if (record.Claimed) continue;

			const auto cached = m_cache.find(oldRelative);
			if (cached == m_cache.end() || cached->second.Hash != hash) continue;

			// Same bytes, different path, and the old path no longer exists: a rename
			record.Claimed  = true;
			entry.Id        = record.Id;
			entry.Settings  = record.Settings;
			m_manifestDirty = true;

			KOI_LOG_INFO("Repaired rename: {} -> {} ({})", oldRelative, entry.VirtualPath, ToString(entry.Id));
			return true;
		}
		return false;
	}

	u64 AssetDatabase::HashFile(const fs::path& file, const CacheRecord* cached) const
	{
		if (cached && cached->Hash != 0) return cached->Hash;

		std::ifstream in(file, std::ios::binary);
		if (!in) return 0;

		const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		return HashBytes(bytes);
	}

	void AssetDatabase::LoadManifest()
	{
		if (!fs::exists(m_manifestPath)) return;

		toml::table root;
		try
		{
			root = toml::parse_file(m_manifestPath.string());
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_ERROR("Failed to parse {}: {}", m_manifestPath.string(), e.description());
			return;
		}

		for (const auto& [key, value] : root)
		{
			const toml::table* entry = value.as_table();
			if (!entry) continue;

			const UUID id = UUIDFromString(key.str());
			if (id.is_nil())
			{
				KOI_LOG_WARN("Skipping manifest entry with unparseable UUID: {}", std::string(key.str()));
				continue;
			}

			const std::string relative = (*entry)["path"].value_or(std::string{});
			if (relative.empty()) continue;

			ManifestRecord record
			{
				.Id   = id,
				.Type = (*entry)["type"].value_or(std::string{}),
			};
			if (const toml::table* settings = (*entry)["settings"].as_table()) record.Settings = *settings;

			m_manifest.emplace(relative, std::move(record));
		}

		KOI_LOG_DEBUG("Loaded {} manifest entries from {}", m_manifest.size(), m_manifestPath.string());
	}

	bool AssetDatabase::SaveManifest() const
	{
		toml::table root;
		for (const auto& [relative, record] : m_manifest)
		{
			toml::table entry;
			entry.insert("path", relative);
			entry.insert("type", record.Type);
			if (!record.Settings.empty()) entry.insert("settings", record.Settings);

			root.insert(ToString(record.Id), std::move(entry));
		}

		std::ofstream out(m_manifestPath, std::ios::trunc);
		if (!out)
		{
			KOI_LOG_ERROR("Failed to write {}", m_manifestPath.string());
			return false;
		}

		out << root;
		m_manifestDirty = false;
		KOI_LOG_INFO("Wrote {} ({} entries)", m_manifestPath.string(), m_manifest.size());
		return true;
	}

	bool AssetDatabase::SaveIdentityManifest(const fs::path& outFile) const
	{
		toml::table root;
		for (const auto& [relative, record] : m_manifest)
		{
			toml::table entry;
			entry.insert("path", relative);
			root.insert(ToString(record.Id), std::move(entry));
		}

		std::ofstream out(outFile, std::ios::trunc);
		if (!out)
		{
			KOI_LOG_ERROR("Failed to write identity manifest {}", outFile.string());
			return false;
		}

		out << root;
		KOI_LOG_DEBUG("Cook: wrote {} identities to {}", m_manifest.size(), outFile.string());
		return true;
	}

	void AssetDatabase::LoadCache()
	{
		if (!fs::exists(m_cachePath)) return;

		toml::table root;
		try
		{
			root = toml::parse_file(m_cachePath.string());
		}
		catch (const toml::parse_error&)
		{
			// Derived data: an unreadable cache costs a rehash, never an error
			KOI_LOG_DEBUG("Discarding unreadable asset cache {}", m_cachePath.string());
			return;
		}

		for (const auto& [key, value] : root)
		{
			const toml::table* entry = value.as_table();
			if (!entry) continue;

			m_cache.emplace(std::string(key.str()), CacheRecord
			{
				.Size    = static_cast<u64>((*entry)["size"].value_or(0ll)),
				.ModTime = (*entry)["mtime"].value_or(0ll),
				.Hash    = static_cast<u64>((*entry)["hash"].value_or(0ll)),
			});
		}
	}

	bool AssetDatabase::SaveCache() const
	{
		std::error_code ec;
		fs::create_directories(m_cachePath.parent_path(), ec);

		toml::table root;
		for (const auto& [relative, record] : m_cache)
		{
			toml::table entry;
			entry.insert("size",  static_cast<i64>(record.Size));
			entry.insert("mtime", record.ModTime);
			entry.insert("hash",  static_cast<i64>(record.Hash));
			root.insert(relative, std::move(entry));
		}

		std::ofstream out(m_cachePath, std::ios::trunc);
		if (!out) return false;

		out << root;
		m_cacheDirty = false;
		return true;
	}

	const AssetEntry* AssetDatabase::Find(const UUID& id) const
	{
		if (id.is_nil()) return nullptr;  // engine content is unidentified; never match it

		const auto it = std::ranges::find(m_entries, id, &AssetEntry::Id);
		return it != m_entries.end() ? &*it : nullptr;
	}

	const AssetEntry* AssetDatabase::Find(std::string_view virtualPath) const
	{
		const auto it = std::ranges::find(m_entries, virtualPath, &AssetEntry::VirtualPath);
		return it != m_entries.end() ? &*it : nullptr;
	}

	u32 AssetDatabase::MissingCount() const
	{
		return static_cast<u32>(std::ranges::count(m_entries, AssetState::Missing, &AssetEntry::State));
	}
}
