#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Core/Serializer.h"

namespace koi
{
	class IComponentType
	{
	public:
		virtual ~IComponentType() = default;

		[[nodiscard]] virtual std::string_view Key() const = 0;  // Sub-table key under [[entity]] in the scene TOML, and the inspector's identity

		[[nodiscard]] virtual std::string_view Category() const { return "Gameplay"; }  // Groups the editor's Add Component menu
		[[nodiscard]] virtual i32  DrawOrder() const { return 0; }  // Where this component sits relative to others in the inspector
		[[nodiscard]] virtual bool Removable() const { return true; }

		[[nodiscard]] virtual bool Has(Entity e) const = 0;
		virtual void Add(Entity e) const    = 0;
		virtual void Remove(Entity e) const = 0;

		virtual void Load(Entity e, const toml::table& entityTable) const = 0;
		virtual void Save(Entity e, toml::table& entityTable) const      = 0;
	};

	// Ensure components have a `kKey` static member for use in scene tables and inspector
	template <typename T>
	concept KeyedComponent = requires { { T::kKey } -> std::convertible_to<std::string_view>; };

	template <typename T> requires Serializable<T> && KeyedComponent<T>
	class ComponentType : public IComponentType
	{
	public:
		// A required component cannot be removed, and defaults itself when the scene omits it
		explicit ComponentType(i32 drawOrder = 0, bool required = false)
			: m_drawOrder(drawOrder)
			, m_required(required)
		{
		}

		[[nodiscard]] std::string_view Key() const override { return T::kKey; }
		[[nodiscard]] i32  DrawOrder() const override { return m_drawOrder; }

		[[nodiscard]] std::string_view Category() const override
		{
			if constexpr (requires { { T::kCategory } -> std::convertible_to<std::string_view>; }) return T::kCategory;
			else return IComponentType::Category();
		}
		[[nodiscard]] bool Removable() const override { return !m_required; }

		[[nodiscard]] bool Has(Entity e) const override { return e.Has<T>(); }
		void Add(Entity e) const override    { e.Set<T>(T{}); }
		void Remove(Entity e) const override { e.Remove<T>(); }

		void Load(Entity e, const toml::table& entityTable) const override
		{
			if (std::optional<T> value = utils::DeserializeFrom<T>(entityTable, T::kKey))
			{
				e.Set<T>(*value);
			}
			else if (m_required)
			{
				e.Set<T>(T{});
			}
		}

		void Save(Entity e, toml::table& entityTable) const override
		{
			if (const T* value = e.TryGet<const T>())
			{
				entityTable.insert(T::kKey, Serializer<T>::Serialize(*value));
			}
		}

	private:
		i32              m_drawOrder;
		bool             m_required;
	};
}
