#include "Koi/Editor/Assets/AssetDatabase.h"
#include "Koi/Engine/Logging/Logger.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace koi::editor
{
	namespace
	{
		constexpr std::string_view kProjectScheme = "project://";

		std::string ParentFolder(std::string_view virtualPath)
		{
			const size_t slash = virtualPath.find_last_of('/');
			return slash == std::string_view::npos
				? std::string(virtualPath)
				: std::string(virtualPath.substr(0, slash));
		}

		std::optional<std::string> ReadText(const fs::path& file)
		{
			std::ifstream in(file, std::ios::binary);
			if (!in) return std::nullopt;

			std::ostringstream buffer;
			buffer << in.rdbuf();
			return buffer.str();
		}

		// Every TOML under the project's content: scenes, materials and mesh sidecars all name assets
		std::vector<fs::path> ProjectTomlFiles(const fs::path& projectSource)
		{
			std::vector<fs::path> files;
			std::error_code ec;
			for (const fs::directory_entry& file : fs::recursive_directory_iterator(projectSource / "Content", ec))
			{
				if (file.is_regular_file() && file.path().extension() == ".toml")
					files.push_back(file.path());
			}
			return files;
		}
	}

	AssetDatabase::AssetDatabase(fs::path engineSource, fs::path projectSource)
		: m_engineSource(std::move(engineSource))
		, m_projectSource(std::move(projectSource))
	{
	}

	void AssetDatabase::Refresh()
	{
		m_entries.clear();
		m_folders.clear();

		ScanRoot(m_engineSource,  "engine",  "Content");
		ScanRoot(m_projectSource, "project", "Content");

		std::ranges::sort(m_entries, {}, &AssetEntry::VirtualPath);

		for (const AssetEntry& entry : m_entries) m_folders.push_back(entry.Folder);
		std::ranges::sort(m_folders);
		m_folders.erase(std::ranges::unique(m_folders).begin(), m_folders.end());

		KOI_LOG_INFO("Asset database: {} asset(s)", m_entries.size());
	}

	void AssetDatabase::ScanRoot(const fs::path& root, std::string_view scheme, std::string_view contentDir)
	{
		const fs::path contentRoot = root / contentDir;
		if (!fs::exists(contentRoot)) return;

		std::error_code ec;
		for (const fs::directory_entry& file : fs::recursive_directory_iterator(contentRoot, ec))
		{
			if (!file.is_regular_file()) continue;

			const AssetTypeInfo* type = FindAssetTypeForFile(file.path().filename().string());
			if (!type) continue;  // .h/.cpp/.bin and anything else unregistered

			const std::string relative = fs::relative(file.path(), root, ec).generic_string();
			if (ec) continue;

			AssetEntry entry
			{
				.VirtualPath = std::string(scheme) + "://" + relative,
				.Name        = file.path().filename().string(),
				.Kind        = type->Kind,
				.DiskPath    = file.path(),
				.Writable    = scheme == "project",
			};
			entry.Folder = ParentFolder(entry.VirtualPath);

			m_entries.push_back(std::move(entry));
		}
	}

	const AssetEntry* AssetDatabase::Find(std::string_view virtualPath) const
	{
		const auto it = std::ranges::find(m_entries, virtualPath, &AssetEntry::VirtualPath);
		return it != m_entries.end() ? &*it : nullptr;
	}

	std::vector<std::string> AssetDatabase::ReferencersOf(std::string_view virtualPath) const
	{
		std::vector<std::string> referencers;
		if (virtualPath.empty()) return referencers;

		for (const fs::path& file : ProjectTomlFiles(m_projectSource))
		{
			const std::optional<std::string> text = ReadText(file);
			if (!text || text->find(virtualPath) == std::string::npos) continue;

			const std::string self = VirtualPathFor(file);
			if (self != virtualPath) referencers.push_back(self);
		}
		return referencers;
	}

	std::string AssetDatabase::VirtualPathFor(const fs::path& diskPath) const
	{
		std::error_code ec;
		const fs::path relative = fs::relative(diskPath, m_projectSource, ec);
		return ec ? std::string() : std::string(kProjectScheme) + relative.generic_string();
	}

	void AssetDatabase::RewriteReferences(std::string_view oldVirtualPath, std::string_view newVirtualPath) const
	{
		for (const fs::path& file : ProjectTomlFiles(m_projectSource))
		{
			std::optional<std::string> text = ReadText(file);
			if (!text || text->find(oldVirtualPath) == std::string::npos) continue;

			size_t pos = 0;
			while ((pos = text->find(oldVirtualPath, pos)) != std::string::npos)
			{
				text->replace(pos, oldVirtualPath.size(), newVirtualPath);
				pos += newVirtualPath.size();
			}

			std::ofstream out(file, std::ios::binary | std::ios::trunc);
			out << *text;
			KOI_LOG_DEBUG("Updated references in {}", file.string());
		}
	}

	bool AssetDatabase::Relocate(const AssetEntry& entry, const fs::path& newDiskPath)
	{
		if (!entry.Writable)
		{
			KOI_LOG_WARN("Cannot move {}: engine content is read-only", entry.VirtualPath);
			return false;
		}

		if (fs::exists(newDiskPath))
		{
			KOI_LOG_ERROR("Cannot move to {}: a file is already there", newDiskPath.string());
			return false;
		}

		const std::string oldVirtualPath = entry.VirtualPath;
		const std::string newVirtualPath = VirtualPathFor(newDiskPath);
		if (newVirtualPath.empty())
		{
			KOI_LOG_ERROR("Cannot move {}: outside the project source tree", oldVirtualPath);
			return false;
		}

		std::error_code ec;
		fs::create_directories(newDiskPath.parent_path(), ec);
		fs::rename(entry.DiskPath, newDiskPath, ec);
		if (ec)
		{
			KOI_LOG_ERROR("Failed to move {} -> {}: {}", oldVirtualPath, newVirtualPath, ec.message());
			return false;
		}

		RewriteReferences(oldVirtualPath, newVirtualPath);
		KOI_LOG_INFO("Moved asset {} -> {}", oldVirtualPath, newVirtualPath);

		Refresh();
		return true;
	}

	bool AssetDatabase::Rename(std::string_view virtualPath, std::string_view newBaseName)
	{
		const AssetEntry* entry = Find(virtualPath);
		if (!entry) return false;

		if (newBaseName.empty() || newBaseName.find_first_of("/\\:*?\"<>|") != std::string_view::npos)
		{
			KOI_LOG_ERROR("'{}' is not a usable file name", newBaseName);
			return false;
		}

		// The extension is what classifies the asset, so a rename never gets to change it
		const std::string_view extension = AssetExtensionOf(entry->Name);
		const fs::path newDiskPath = entry->DiskPath.parent_path() / (std::string(newBaseName) + std::string(extension));

		return Relocate(*entry, newDiskPath);
	}

	bool AssetDatabase::Move(std::string_view virtualPath, std::string_view newFolder)
	{
		const AssetEntry* entry = Find(virtualPath);
		if (!entry) return false;

		if (!newFolder.starts_with(kProjectScheme))
		{
			KOI_LOG_ERROR("Cannot move an asset into '{}': only project folders can be written to", newFolder);
			return false;
		}

		const fs::path newDir = m_projectSource / fs::path(newFolder.substr(kProjectScheme.size()));
		if (newDir == entry->DiskPath.parent_path()) return false;

		return Relocate(*entry, newDir / entry->Name);
	}

	bool AssetDatabase::Delete(std::string_view virtualPath)
	{
		const AssetEntry* entry = Find(virtualPath);
		if (!entry) return false;

		if (!entry->Writable)
		{
			KOI_LOG_WARN("Cannot delete {}: engine content is read-only", entry->VirtualPath);
			return false;
		}

		std::error_code ec;
		if (!fs::remove(entry->DiskPath, ec))
		{
			KOI_LOG_ERROR("Failed to delete {}: {}", entry->DiskPath.string(), ec.message());
			return false;
		}

		KOI_LOG_INFO("Deleted asset {}", entry->VirtualPath);
		Refresh();
		return true;
	}
}
