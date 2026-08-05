#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/ECS/Components.h"
#include <flecs.h>
#include <toml++/toml.hpp>
#include <optional>
#include <filesystem>

namespace koi
{
	class GfxDevice;
	namespace fs = std::filesystem;

	using ECS      = flecs::world;
	using Entity   = flecs::entity;
	using EntityID = flecs::entity_t;


	class World
	{
	public:
		World();
		World(const World&) = delete;
		World& operator=(const World&) = delete;
		World(World&&) noexcept = default;
		World& operator=(World&&) noexcept = default;

		[[nodiscard]] static std::optional<World> Load(const fs::path& path, GfxDevice& device);

		bool Save(const fs::path& path);

		bool LoadFromTable(const toml::table& scene, GfxDevice& device);
		[[nodiscard]] toml::table SaveToTable();

		// Destroys every entity and releases the assets they held references on
		void Clear(GfxDevice& device);

		void Update(f32 dt);

		[[nodiscard]] const AABB& GetBounds() const { return m_bounds; }

		// Projects the primary camera entity for a surface of the given aspect and returns
		// it. Null if the world has no primary camera. Aspect is a property of the render
		// target, so this is called by whoever resolved that target, once per frame
		[[nodiscard]] const Camera* ResolveCamera(f32 aspect);

		[[nodiscard]] const GPUMesh* GetMesh(MeshHandle handle) const;

		[[nodiscard]] ECS&       GetECS()       { return m_ecs; }
		[[nodiscard]] const ECS& GetECS() const { return m_ecs; }

	private:
		ECS    m_ecs;
		Entity m_root;
		AABB   m_bounds;
		Camera m_camera;  // derived from the primary camera entity by ResolveCamera
	};
}
