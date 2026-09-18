#pragma once
#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Math/Transform.h"
#include <string_view>

namespace koi
{
	class EntityBuilder
	{
	public:
		explicit EntityBuilder(Entity entity) : m_entity(entity) {}

		EntityBuilder& WithMesh(std::string_view virtualPath);
		EntityBuilder& WithMaterial(std::string_view virtualPath);

		EntityBuilder& At(const Vector3& position);
		EntityBuilder& Rotated(const Quaternion& rotation);
		EntityBuilder& Scaled(const Vector3& scale);
		EntityBuilder& Scaled(f32 uniform) { return Scaled(Vector3(uniform, uniform, uniform)); }

		template<typename T>
		EntityBuilder& With(T&& component)
		{
			m_entity.set<std::remove_cvref_t<T>>(std::forward<T>(component));
			return *this;
		}

		[[nodiscard]] Entity Build() const { return m_entity; }
		operator Entity() const { return m_entity; }

	private:
		Entity    m_entity;
		Transform m_transform;  // written whole on every change, which stays valid while flecs is deferring
	};

	// Creates an entity with a Transform. Names are unique per world and looked up by scene files, so runtime spawns should stay unnamed
	[[nodiscard]] EntityBuilder Spawn(World& world, std::string_view name = {});
}
