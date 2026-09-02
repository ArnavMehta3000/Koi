#pragma once
#include "Koi/Engine/Assets/AssetKey.h"
#include <unordered_map>

namespace koi
{
	// uuid <-> virtual path mapping
	class AssetIdentity
	{
	public:
		[[nodiscard]] static AssetIdentity& Get();

		void Clear();
		void Set(const UUID& id, std::string virtualPath);

		// Reads an identity manifest (uuid -> path, no settings)
		bool LoadFromVfs(std::string_view manifestPath);

		// Where to load key from, prioritized underlying uri
		[[nodiscard]] std::string_view PathFor(const AssetKey& key) const;
		[[nodiscard]] AssetKey KeyForPath(std::string_view virtualPath) const;

		[[nodiscard]] inline size_t Size() const { return m_pathById.size(); }

	private:
		std::unordered_map<UUID, std::string> m_pathById;
		std::unordered_map<std::string, UUID> m_idByPath;
	};
}
