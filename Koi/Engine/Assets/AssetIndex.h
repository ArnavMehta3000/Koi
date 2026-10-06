#pragma once
#include "Koi/Engine/Assets/AssetFile.h"
#include <unordered_map>

namespace koi
{
	struct AssetIndexEntry
	{
		UUID          Id;
		AssetFileType Type = AssetFileType::Unknown;
		std::string   VirtualPath;
	};

	// Where every binary asset lives, keyed by UUID. Built from the headers alone, so no asset is loaded
	class AssetIndex : public NonCopyable
	{
	public:
		AssetIndex() = default;

		// Rescans the Content/ folder under every mounted disk root
		void Rebuild();
		void Clear();

		// False when another file already owns this id
		bool Add(const UUID& id, AssetFileType type, std::string virtualPath);

		[[nodiscard]] const AssetIndexEntry* Find(const UUID& id) const;
		[[nodiscard]] const AssetIndexEntry* FindByPath(std::string_view virtualPath) const;
		[[nodiscard]] size_t Count() const { return m_entries.size(); }

	private:
		void ScanRoot(std::string_view scheme, const fs::path& root);

	private:
		std::unordered_map<UUID, AssetIndexEntry> m_entries;
		std::unordered_map<std::string, UUID>     m_idsByPath;
	};
}
