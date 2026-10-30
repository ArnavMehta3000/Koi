#pragma once
#include "Koi/Engine/Assets/AssetKey.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/World.h"
#include <optional>

namespace koi::editor
{
	// A component on an entity
	template <typename T>
	struct ComponentOf
	{
		using Value = T;

		explicit ComponentOf(Entity target) : Target(target) {}

		[[nodiscard]] std::optional<T> Read() const
		{
			const T* value = Target.is_alive() ? Target.try_get<T>() : nullptr;
			return value ? std::optional<T>(*value) : std::nullopt;
		}

		void Write(const std::optional<T>& value) const
		{
			if (!Target.is_alive()) return;

			if (value)
			{
				Target.set<T>(*value);
			}
			else
			{
				Target.remove<T>();
			}
		}

		[[nodiscard]] u64 Identity() const { return Target.id(); }

		Entity Target;
	};

	// Asset refs hold a handle the cache may evict, so they're captured by key and reacquired on write
	template <>
	struct ComponentOf<MeshRef>
	{
		using Value = AssetKey;

		explicit ComponentOf(Entity target) : Target(target) {}

		[[nodiscard]] std::optional<AssetKey> Read() const;
		void Write(const std::optional<AssetKey>& key) const;
		[[nodiscard]] u64 Identity() const { return Target.id(); }

		Entity Target;
	};

	template <>
	struct ComponentOf<MaterialRef>
	{
		using Value = AssetKey;

		explicit ComponentOf(Entity target) : Target(target) {}

		[[nodiscard]] std::optional<AssetKey> Read() const;
		void Write(const std::optional<AssetKey>& key) const;
		[[nodiscard]] u64 Identity() const { return Target.id(); }

		Entity Target;
	};
}
