#pragma once
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <unordered_map>

namespace koi::editor
{
	namespace fs = std::filesystem;

	struct AssetEntry
	{
		UUID        Id;
		std::string VirtualPath;   // "project://Assets/Models/DamagedHelmet.gltf"
		std::string Name;          // filename, for display and search
		std::string Folder;        // virtual parent dir, for the folder tree
		AssetKind   Kind  = AssetKind::Unknown;
		AssetState  State = AssetState::Unloaded;
		fs::path    DiskPath;
		toml::table Settings;      // per-asset config, round-tripped verbatim

		// How the caches address this asset: by uuid for project content, by path for engine content
		[[nodiscard]] AssetKey Key() const
		{
			return Id.is_nil() ? AssetKey::FromUri(VirtualPath) : AssetKey(Id);
		}
	};

	class AssetDatabase  // On disk content database
	{
	public:
		AssetDatabase(fs::path engineSource, fs::path projectSource);

		void Refresh();
		bool SaveManifest() const;

		// Identity only - uuid -> path, no settings. Cook() ships this so a standalone build
		// can resolve the uuids in a scene without the editor's live scan
		bool SaveIdentityManifest(const fs::path& outFile) const;

		[[nodiscard]] std::span<const AssetEntry>  All()     const { return m_entries; }
		[[nodiscard]] std::span<const std::string> Folders() const { return m_folders; }

		[[nodiscard]] const AssetEntry* Find(const UUID& id) const;
		[[nodiscard]] const AssetEntry* Find(std::string_view virtualPath) const;

		// Entries whose manifest path no longer resolves to a file
		[[nodiscard]] u32 MissingCount() const;

		// Asset dependencies
		[[nodiscard]] std::span<const AssetKey> DependenciesOf(const UUID& id) const;
		[[nodiscard]] std::span<const UUID> ReferencersOf(const UUID& id) const;
		[[nodiscard]] UUID ResolveDependency(const AssetKey& key) const;

		bool Rename(const UUID& id, std::string_view newBaseName);
		bool Move(const UUID& id, std::string_view newFolder);
		bool Delete(const UUID& id, bool force = false);

	private:
		struct CacheRecord
		{
			u64 Size     = 0;
			i64 ModTime  = 0;
			u64 Hash     = 0;
		};

		// Manifest identity carried across a Refresh, keyed by the path it was last seen at
		struct ManifestRecord
		{
			UUID                  Id;
			std::string           Type;
			toml::table           Settings;
			std::vector<AssetKey> Deps;
			bool                  DepsKnown = false;  // false forces a rescan, so a pre-deps manifest migrates
			bool                  Claimed   = false;
		};

		void ScanRoot(const fs::path& root, std::string_view scheme, std::string_view contentDir);
		void ReconcileEntry(AssetEntry& entry, const std::string& relative, const AssetTypeInfo& type, const fs::directory_entry& file);
		void LoadManifest();
		void LoadCache();
		bool SaveCache() const;
		bool TryRepairRename(AssetEntry& entry, u64 hash);  // Try fix out-of-editor rename

		void BuildDependencyIndex();

		// The shared half of Rename and Move: relocate the file, then carry the manifest and
		// cache records over to its new path so the next Refresh needs no hash repair
		bool Relocate(const UUID& id, const fs::path& newDiskPath);

		[[nodiscard]] std::string RelativeToProject(const fs::path& diskPath) const;

		[[nodiscard]] u64 HashFile(const fs::path& file, const CacheRecord* cached) const;

	private:
		fs::path m_engineSource;
		fs::path m_projectSource;
		fs::path m_manifestPath;
		fs::path m_cachePath;

		std::vector<AssetEntry>  m_entries;
		std::vector<std::string> m_folders;

		std::unordered_map<std::string, ManifestRecord> m_manifest;  // relative path -> record
		std::unordered_map<std::string, CacheRecord>    m_cache;     // relative path -> record

		std::unordered_map<UUID, std::vector<AssetKey>> m_deps;         // asset -> what it names
		std::unordered_map<UUID, std::vector<UUID>>     m_referencers;  // asset -> who names it

		mutable bool m_manifestDirty = false;
		mutable bool m_cacheDirty    = false;
	};
}
