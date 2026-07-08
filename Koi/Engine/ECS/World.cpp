#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <cassert>

namespace koi
{
	World::World()
	{
		m_bounds.Min = math::float3(FLT_MAX, FLT_MAX, FLT_MAX);
		m_bounds.Max = math::float3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		// Cascade query: parents are recomputed before their children, so a single
		// pass produces correct world transforms with no manual dirty tracking.
		m_transformQuery = m_ecs.query_builder<const Transform, const WorldTransform*, WorldTransform>()
			.term_at(1).parent().cascade()
			.build();
	}

	std::optional<World> World::Load(const fs::path& /*path*/, GfxDevice& device)
	{
		// TODO: scene TOML parsing is being replaced by Flecs JSON import/export
		// (see project_scene_serialization_future). Hardcoded single mesh for now.
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		World world;
		constexpr const char* kMeshPath = "project://Assets/Models/DamagedHelmet.gltf";

		KOI_LOG_INFO("Loading world (hardcoded): {}", kMeshPath);

		flecs::entity e = world.m_ecs.entity("DamagedHelmet");
		e.set<Transform>({});
		e.set<WorldTransform>({});

		MeshHandle handle = world.m_meshRegistry.GetOrLoad(kMeshPath, device, &world.m_bounds);
		e.set<MeshRef>({ kMeshPath, handle });

		const math::float3 center = world.m_bounds.Center();
		const f32 radius = world.m_bounds.Radius();

		world.m_camera.SetProjection(0.8f, 16.0f / 9.0f, 0.05f, 1000.0f);
		world.m_camera.SetView(center + math::float3(0.0f, 0.0f, -radius * 2.5f), center);

		return world;
	}

	void World::Update(f32 dt)
	{
		ZoneScoped;
		m_ecs.progress(dt);

		m_transformQuery.each([](const Transform& local, const WorldTransform* parentWorld, WorldTransform& worldXform)
		{
			math::float4x4 localMat = local.ToMatrix();
			worldXform.Value = parentWorld ? localMat * parentWorld->Value : localMat;
		});
	}

	void World::Release(GfxDevice& device)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		m_meshRegistry.Release(device);
	}
}
