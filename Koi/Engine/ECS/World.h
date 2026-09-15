#pragma once
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Camera.h"
#include <flecs.h>
#include <toml++/toml.hpp>
#include <filesystem>
#include <optional>

namespace koi
{
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

		[[nodiscard]] static std::optional<World> Load(std::string_view virtualPath);

		[[nodiscard]] inline const std::string& GetSourcePath() const { return m_sourcePath; }
		[[nodiscard]] inline const BoundingBox& GetBounds()     const { return m_bounds;     }
		[[nodiscard]] inline ECS&               GetECS()              { return m_ecs;        }
		[[nodiscard]] inline const ECS&         GetECS()        const { return m_ecs;        }

		// Writes back to the scene file this world was loaded from
		bool Save();
		bool LoadFromTable(const toml::table& scene);
		[[nodiscard]] toml::table SaveToTable();
		void Clear(); // Destroys every entity. The assets they referenced are released by component hooks

		void Update(f32 dt);

		// Projects the primary camera entity for a surface of the given aspect and returns it
		// Null if the world has no primary camera
		[[nodiscard]] const Camera* ResolveCamera(f32 aspect);
		[[nodiscard]] const GPUMesh* GetMesh(MeshHandle handle) const;


	private:
		void ConfigureComponentHooks();

	private:
		ECS    m_ecs;
		Entity m_root;
		std::string m_sourcePath;  // virtual path this world was loaded from, empty if none
		BoundingBox m_bounds{ {}, {} };  // zero extents until meshes resolve
		Camera m_camera;  // derived from the primary camera entity by ResolveCamera
	};
}
