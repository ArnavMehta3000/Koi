#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Runtime/Serializer.h"
#include <span>
#include <string_view>
#include <vector>

namespace koi
{
	class GfxDevice;

	struct ComponentInfo
	{
		using LoadFn = void(Entity, const toml::table& entityTable, std::string_view key, GfxDevice&);
		using SaveFn = void(Entity, toml::table& entityTable, std::string_view key);
		using DrawFn = void(Entity);

		std::string_view Key;  // Sub-table key under [[entity]] in TOML

		LoadFn* Load = nullptr;
		SaveFn* Save = nullptr;
		DrawFn* Draw = nullptr;
	};

	class ComponentRegistry
	{
	public:
		[[nodiscard]] static ComponentRegistry& Get();

		template <Serializable T, bool Required = false>
		void Register(std::string_view key, ComponentInfo::DrawFn* draw = nullptr)
		{
			m_components.push_back(ComponentInfo
			{
				.Key = key,

				.Load = [](Entity e, const toml::table& table, std::string_view k, GfxDevice&)
				{
					if (std::optional<T> value = DeserializeFrom<T>(table, k))
						e.set<T>(*value);
					else if constexpr (Required)
						e.set<T>(T{});
				},

				.Save = [](Entity e, toml::table& table, std::string_view k)
				{
					if (const T* value = e.try_get<T>())
						table.insert(k, Serializer<T>::Serialize(*value));
				},

				.Draw = draw
			});
		}

		void Register(ComponentInfo info) { m_components.push_back(info); }

		[[nodiscard]] std::span<const ComponentInfo> All() const { return m_components; }

	private:
		ComponentRegistry();

	private:
		std::vector<ComponentInfo> m_components;
	};
}
