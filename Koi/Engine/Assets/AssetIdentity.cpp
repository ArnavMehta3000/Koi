#include "Koi/Engine/Assets/AssetIdentity.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include <toml++/toml.hpp>

namespace koi
{
	AssetIdentity& AssetIdentity::Get()
	{
		static AssetIdentity instance;
		return instance;
	}

	void AssetIdentity::Clear()
	{
		m_pathById.clear();
		m_idByPath.clear();
	}

	void AssetIdentity::Set(const UUID& id, std::string virtualPath)
	{
		if (id.is_nil() || virtualPath.empty()) return;  // engine content carries no identity

		m_idByPath.insert_or_assign(virtualPath, id);
		m_pathById.insert_or_assign(id, std::move(virtualPath));
	}

	std::string_view AssetIdentity::PathFor(const AssetKey& key) const
	{
		if (key.IsUri()) return key.Uri();

		// Try findng by id
		const auto it = m_pathById.find(key.Id());
		return it != m_pathById.end() ? std::string_view(it->second) : std::string_view();
	}

	AssetKey AssetIdentity::KeyForPath(std::string_view virtualPath) const
	{
		if (virtualPath.empty()) return {};

		const auto it = m_idByPath.find(std::string(virtualPath));
		return it != m_idByPath.end() ? AssetKey(it->second) : AssetKey::FromUri(std::string(virtualPath));
	}

	bool AssetIdentity::LoadFromVfs(std::string_view manifestPath)
	{
		const std::optional<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(manifestPath);
		if (!bytes)
		{
			KOI_LOG_WARN("No asset manifest at '{}'; scenes will fall back to their path hints", manifestPath);
			return false;
		}

		toml::table root;
		try
		{
			root = toml::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_ERROR("Failed to parse asset manifest '{}': {}", manifestPath, e.description());
			return false;
		}

		Clear();

		for (const auto& [key, value] : root)
		{
			const toml::table* entry = value.as_table();
			if (!entry) continue;

			const UUID id = UUIDFromString(key.str());
			const std::string relative = (*entry)["path"].value_or(std::string{});
			if (id.is_nil() || relative.empty()) continue;

			// Manifest paths are project-relative; the cache wants the addressable form
			Set(id, "project://" + relative);
		}

		KOI_LOG_INFO("Loaded {} asset identities from '{}'", m_pathById.size(), manifestPath);
		return true;
	}
}
