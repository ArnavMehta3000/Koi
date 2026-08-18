#pragma once
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/Core/UUID.h"
#include <string>
#include <unordered_map>

namespace koi
{
	// uuid <-> virtual path | Seen only by the engine
	// Populated live from AssetDatabase in the editor, and from the manifest Cook()
	class AssetIdentity
	{
	public:
		[[nodiscard]] static AssetIdentity& Get();

		void Clear();
		void Set(const UUID& id, std::string virtualPath);

		// Reads an identity manifest (uuid -> path, no settings)
		bool LoadFromVfs(std::string_view manifestPath);

		// Empty when the id is unknown, so callers fall back to the scene's path hint
		[[nodiscard]] std::string_view PathFor(const UUID& id) const;

		[[nodiscard]] size_t Size() const { return m_pathById.size(); }

#if KOI_WITH_EDITOR
		// Exist just so that on saving a scene, it can stamp the UUID of an asset the cache only know by path
		[[nodiscard]] UUID IdFor(std::string_view virtualPath) const;
#endif

	private:
		std::unordered_map<UUID, std::string> m_pathById;

#if KOI_WITH_EDITOR
		std::unordered_map<std::string, UUID> m_idByPath;
#endif
	};
}
