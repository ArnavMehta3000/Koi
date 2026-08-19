#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Runtime/Serializer.h"
#include <span>
#include <vector>

namespace koi
{
	class GfxDevice;

	struct ComponentInfo
	{
		using LoadFn   = void(Entity, const toml::table& entityTable, std::string_view key, GfxDevice&);
		using SaveFn   = void(Entity, toml::table& entityTable, std::string_view key);
		using DrawFn   = void(Entity);
		using HasFn    = bool(Entity);
		using AddFn    = void(Entity, GfxDevice&);
		using RemoveFn = void(Entity);

		std::string_view Key;  // Sub-table key under [[entity]] in TOML

		// Where this component draws relative to others in the inspector
		i32  DrawOrder = 0;
		bool Removable = true;  // Transform component is the exception

		LoadFn*   Load   = nullptr;
		SaveFn*   Save   = nullptr;
		DrawFn*   Draw   = nullptr;
		HasFn*    Has    = nullptr;
		AddFn*    Add    = nullptr;
		RemoveFn* Remove = nullptr;
	};

	class ComponentRegistry
	{
	public:
		[[nodiscard]] static ComponentRegistry& Get();

		template <Serializable T, bool Required = false>
		void Register(std::string_view key, ComponentInfo::DrawFn* draw = nullptr, i32 drawOrder = 0)
		{
			m_components.push_back(ComponentInfo
			{
				.Key       = key,
				.DrawOrder = drawOrder,
				.Removable = !Required,

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

				.Draw   = draw,
				.Has    = [](Entity e) { return e.has<T>(); },
				.Add    = [](Entity e, GfxDevice&) { e.set<T>(T{}); },
				.Remove = [](Entity e) { e.remove<T>(); },
			});
		}

		void Register(ComponentInfo info) { m_components.push_back(std::move(info)); }

		[[nodiscard]] std::span<const ComponentInfo> All() const { return m_components; }

	private:
		ComponentRegistry();

	private:
		std::vector<ComponentInfo> m_components;
	};
}
