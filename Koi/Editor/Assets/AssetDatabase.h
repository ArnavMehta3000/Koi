#pragma once
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace koi::editor
{
	namespace fs = std::filesystem;

	struct AssetEntry
	{
		std::string VirtualPath;   // "project://Content/Models/DamagedHelmet/DamagedHelmet.kmesh"; also the cache key
		std::string Name;          // filename, for display and search
		std::string Folder;        // virtual parent dir, for the folder tree
		AssetKind   Kind     = AssetKind::Unknown;
		fs::path    DiskPath;
		bool        Writable = false;  // project content; engine content is read-only

		[[nodiscard]] AssetKey Key() const { return AssetKey(VirtualPath); }
	};

	// A scan of the engine and project content trees. Identity is the virtual path, so moving a
	// file means rewriting the TOML files that name it
	class AssetDatabase
	{
	public:
		AssetDatabase(fs::path engineSource, fs::path projectSource);

		void Refresh();

		[[nodiscard]] std::span<const AssetEntry>  All()     const { return m_entries; }
		[[nodiscard]] std::span<const std::string> Folders() const { return m_folders; }

		// Invalidated by the next Refresh, which rebuilds the entry list wholesale
		[[nodiscard]] const AssetEntry* Find(std::string_view virtualPath) const;

		// Project TOML files whose text names this asset
		[[nodiscard]] std::vector<std::string> ReferencersOf(std::string_view virtualPath) const;

		bool Rename(std::string_view virtualPath, std::string_view newBaseName);
		bool Move(std::string_view virtualPath, std::string_view newFolder);
		bool Delete(std::string_view virtualPath);

	private:
		void ScanRoot(const fs::path& root, std::string_view scheme, std::string_view contentDir);

		// Moves the file, then points every project TOML that named the old path at the new one
		bool Relocate(const AssetEntry& entry, const fs::path& newDiskPath);
		void RewriteReferences(std::string_view oldVirtualPath, std::string_view newVirtualPath) const;

		[[nodiscard]] std::string VirtualPathFor(const fs::path& diskPath) const;

	private:
		fs::path m_engineSource;
		fs::path m_projectSource;

		std::vector<AssetEntry>  m_entries;
		std::vector<std::string> m_folders;
	};
}
