#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Runtime/Serializer.h"

namespace koi
{
	class IComponentType
	{
	public:
		virtual ~IComponentType() = default;

		[[nodiscard]] virtual std::string_view Key() const = 0;  // Sub-table key under [[entity]] in the scene TOML, and the inspector's identity

		[[nodiscard]] virtual i32  DrawOrder() const { return 0; }  // Where this component sits relative to others in the inspector
		[[nodiscard]] virtual bool Removable() const { return true; }

		[[nodiscard]] virtual bool Has(Entity e) const = 0;
		virtual void Add(Entity e) const    = 0;
		virtual void Remove(Entity e) const = 0;

		virtual void Load(Entity e, const toml::table& entityTable) const = 0;
		virtual void Save(Entity e, toml::table& entityTable) const      = 0;
	};

	template <Serializable T>
	class ComponentType : public IComponentType
	{
	public:
		// A required component cannot be removed, and defaults itself when the scene omits it
		ComponentType(std::string_view key, i32 drawOrder = 0, bool required = false)
			: m_key(key)
			, m_drawOrder(drawOrder)
			, m_required(required)
		{
		}

		[[nodiscard]] std::string_view Key() const override { return m_key; }
		[[nodiscard]] i32  DrawOrder() const override { return m_drawOrder; }
		[[nodiscard]] bool Removable() const override { return !m_required; }

		[[nodiscard]] bool Has(Entity e) const override { return e.has<T>(); }
		void Add(Entity e) const override    { e.set<T>(T{}); }
		void Remove(Entity e) const override { e.remove<T>(); }

		void Load(Entity e, const toml::table& entityTable) const override
		{
			if (std::optional<T> value = DeserializeFrom<T>(entityTable, m_key))
			{
				e.set<T>(*value);
			}
			else if (m_required)
			{
				e.set<T>(T{});
			}
		}

		void Save(Entity e, toml::table& entityTable) const override
		{
			if (const T* value = e.try_get<T>())
			{
				entityTable.insert(m_key, Serializer<T>::Serialize(*value));
			}
		}

	private:
		std::string_view m_key;
		i32              m_drawOrder;
		bool             m_required;
	};
}
