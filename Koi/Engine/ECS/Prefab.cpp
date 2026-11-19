#include "Koi/Engine/ECS/Prefab.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>

namespace koi
{
	namespace
	{
		AssetRegistry&              Assets()  { return Engine::Get().GetSubsystem<AssetRegistry>(); }
		AssetCache<PrefabTemplate>& Prefabs() { return Assets().Cache<PrefabTemplate>(); }
		GfxDevice&                  Device()  { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }
	}

	PrefabRegistry& PrefabRegistry::Get()
	{
		static PrefabRegistry s_instance;
		return s_instance;
	}

	void PrefabRegistry::Register(std::string_view className, Factory factory)
	{
		if (Contains(className))
		{
			KOI_LOG(ECS, Error, "Prefab class '{}' is already registered; ignoring the second registration", className);
			return;
		}

		m_factories.emplace_back(std::string(className), factory);
		KOI_LOG(ECS, Debug, "Registered prefab class '{}'", className);
	}

	bool PrefabRegistry::Contains(std::string_view className) const
	{
		return std::ranges::contains(m_factories, className, [](const auto& entry) { return std::string_view(entry.first); });
	}

	std::unique_ptr<IPrefabTemplate> PrefabRegistry::Create(std::string_view className) const
	{
		const auto it = std::ranges::find(m_factories, className, [](const auto& entry) { return std::string_view(entry.first); });
		return it != m_factories.end() ? it->second() : nullptr;
	}

	PrefabHandle internal::AcquirePrefab(std::string_view idOrPath)
	{
		std::string path(idOrPath);
		if (const UUID id = utils::UUIDFromString(idOrPath); !id.is_nil())
		{
			const AssetIndexEntry* entry = Assets().Index().Find(id);
			if (!entry)
			{
				KOI_LOG(Assets, Warn, "Prefab {} isn't in the project anymore", idOrPath);
				return {};
			}
			path = entry->VirtualPath;
		}
		return Prefabs().Acquire(Device(), AssetKey(std::move(path)));
	}

	PrefabHandle internal::AddRefPrefab(PrefabHandle handle)
	{
		return Prefabs().AddRef(handle);
	}

	void internal::ReleasePrefab(PrefabHandle handle)
	{
		// A copy can outlive the asset registry (editor undo history), which leaves nothing to release
		if (Prefabs().Get(handle)) Prefabs().Release(Device(), handle);
	}

	const IPrefabTemplate* internal::FindPrefab(PrefabHandle handle)
	{
		const PrefabTemplate* prefab = Prefabs().Get(handle);
		return prefab ? prefab->Data.get() : nullptr;
	}

	std::string internal::PrefabIdOf(PrefabHandle handle)
	{
		if (!handle) return {};

		const AssetKey key = Prefabs().GetKey(handle);
		const AssetIndexEntry* entry = Assets().Index().FindByPath(key.Path());
		return entry ? utils::ToString(entry->Id) : std::string(key.Path());
	}

	void internal::WarnUndeclaredComponents(const toml::table& file, std::string_view path, std::span<const std::string_view> declared)
	{
		for (const auto& [key, value] : file)
		{
			if (value.is_table() && !std::ranges::contains(declared, key.str()))
			{
				KOI_LOG(Assets, Warn, "Prefab {} has a '{}' table its class doesn't declare; it's ignored", path, key.str());
			}
		}
	}
}
