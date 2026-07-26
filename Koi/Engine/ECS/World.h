#pragma once
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Graphics/Camera.h"
#include "Koi/Engine/Graphics/MeshRegistry.h"
#include "Koi/Engine/ECS/Components.h"
#include <flecs.h>
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

		void Update(f32 dt);
		void Release(GfxDevice& device);

		[[nodiscard]] const AABB& GetBounds() const { return m_bounds; }

		[[nodiscard]] Camera&       GetCamera()       { return m_camera; }
		[[nodiscard]] const Camera& GetCamera() const { return m_camera; }

		[[nodiscard]] const GPUMesh* GetMesh(MeshHandle handle) const { return m_meshRegistry.Get(handle); }

		[[nodiscard]] ECS&       GetECS()       { return m_ecs; }
		[[nodiscard]] const ECS& GetECS() const { return m_ecs; }

	private:
		ECS          m_ecs;
		Entity       m_root;
		MeshRegistry m_meshRegistry;
		AABB         m_bounds;
		Camera       m_camera;
	};
}
