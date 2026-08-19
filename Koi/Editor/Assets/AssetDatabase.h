#pragma once
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Core/UUID.h"
#include <toml++/toml.hpp>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace koi::editor
{
	namespace fs = std::filesystem;

	// One browsable asset. Engine content has a nil Id - identity is project-only, because
	// engine assets are referenced by path from C++ and never renamed by a user
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
	};

	// What content exists, as opposed to what is resident on the GPU (that is AssetCache).
	// Editor-only: a cooked runtime has its paths baked into the scene TOML already.
	//
	// Two tiers, and the rule between them is load-bearing:
	//   <project>/AssetRegistry.toml  committed   uuid -> path + settings
	//   <project>/.koi/               gitignored  derived; deleting it costs a rescan, never data
	class AssetDatabase
	{
	public:
		AssetDatabase(fs::path engineSource, fs::path projectSource);

		// Rescans both roots, reconciles against the manifest, and writes it back if anything
		// changed. Safe to call repeatedly - unchanged files are skipped via the .koi cache
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
			UUID        Id;
			std::string Type;
			toml::table Settings;
			bool        Claimed = false;
		};

		void ScanRoot(const fs::path& root, std::string_view scheme, std::string_view contentDir);

		// Gives a scanned project file its identity: existing manifest entry, repaired rename,
		// or a freshly minted UUID
		void ReconcileEntry(AssetEntry& entry, const std::string& relative,
		                    const AssetTypeInfo& type, const fs::directory_entry& file);
		void LoadManifest();
		void LoadCache();
		bool SaveCache() const;

		// Adopts the identity of a manifest entry whose file vanished but whose cached content
		// hash matches this one - an out-of-editor rename, repaired
		bool TryRepairRename(AssetEntry& entry, u64 hash);

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

		mutable bool m_manifestDirty = false;
		mutable bool m_cacheDirty    = false;
	};
}
