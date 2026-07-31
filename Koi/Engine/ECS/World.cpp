#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <vector>

namespace koi
{
	World::World()
	{
		ZoneScoped;

		m_bounds.Min = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		m_bounds.Max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		// Add root entity, disabled by default. Every other entity is parented to this
		m_root = m_ecs.entity("WorldRoot");
	}

	std::optional<World> World::Load(const fs::path& path, GfxDevice& device)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		const std::string virtualPath = "project://" + path.generic_string();

		const std::optional<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(virtualPath);
		if (!bytes)
		{
			KOI_LOG_ERROR("Failed to read scene file: {}", virtualPath);
			return std::nullopt;
		}

		const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());

		try
		{
			const toml::table scene = toml::parse(text);

			World world;
			world.LoadFromTable(scene, device);

			KOI_LOG_INFO("Loaded scene '{}'", virtualPath);
			return world;
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_ERROR("Failed to parse scene '{}': {}", virtualPath, e.description());
			return std::nullopt;
		}
	}

	bool World::LoadFromTable(const toml::table& scene, GfxDevice& device)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		const toml::array* entities = scene["entity"].as_array();
		if (!entities)
		{
			KOI_LOG_WARN("Scene has no [[entity]] array; loading empty world");
			return false;
		}

		// Pass 1: create each entity + its components, deferring parent links (by name)
		// so an entity can reference a parent declared later in the file
		struct PendingParent { Entity Child; std::string ParentName; };
		std::vector<PendingParent> pendingParents;

		const std::span<const ComponentInfo> components = ComponentRegistry::Get().All();
		u32 index = 0;
		for (const toml::node& node : *entities)
		{
			const u32 i = index++;

			const toml::table* entityTable = node.as_table();
			if (!entityTable) continue;

			// Create entity
			const std::string name = (*entityTable)["name"].value_or<std::string>("Entity_" + std::to_string(i));
			Entity e = m_ecs.entity(name.c_str());

			// Load components (use registry for this)
			for (const ComponentInfo& component : components)
				component.Load(e, *entityTable, component.Key, device);

			// Configure parents if they exist
			if (const std::string parentName = (*entityTable)["parent"].value_or(std::string()); !parentName.empty())
				pendingParents.push_back({ e, parentName });
		}

		// Pass 2: resolve parent links now that every named entity exists
		for (const PendingParent& link : pendingParents)
		{
			if (const Entity parent = m_ecs.lookup(link.ParentName.c_str()))
				link.Child.child_of(parent);
			else
				KOI_LOG_WARN("Scene entity parent '{}' not found", link.ParentName);
		}

		// World bounds come from whichever meshes ended up resident
		const AssetCache<GPUMesh>& meshes = Engine::Get().GetAssets().Cache<GPUMesh>();
		m_ecs.each<MeshRef>([&](Entity, MeshRef& mesh)
		{
			if (const GPUMesh* gpuMesh = meshes.Get(mesh.Handle))
				m_bounds.Expand(gpuMesh->Bounds);
		});

		// If no deserialized camera exists, create one centered on the scene world bounds
		bool hasCamera = false;
		m_ecs.query<const CameraComponent>().each([&](const CameraComponent&) { hasCamera = true; });
		if (!hasCamera)
		{
			Transform camXform;
			if (m_bounds.Min.x <= m_bounds.Max.x)
			{
				const Vector3 center = m_bounds.Center();
				const f32 radius     = m_bounds.Radius();
				camXform.Position    = center + Vector3(0.0f, 0.0f, radius * 2.5f);
			}
			else
			{
				camXform.Position = Vector3(0.0f, 1.0f, 5.0f);
			}

			m_ecs.entity("Camera")
				.set<Transform>(camXform)
				.set<CameraComponent>({});
		}

		KOI_LOG_INFO("Built world from scene table ({} entities)", entities->size());
		return true;
	}

	bool World::Save(const fs::path& path)
	{
		ZoneScoped;

		const std::string virtualPath = "project://" + path.generic_string();

		const toml::table root = SaveToTable();

		// project:// must resolve to a real file (editor source tree) to write back
		const std::optional<fs::path> diskPath = VirtualFileSystem::Get().TryResolveToDisk(virtualPath);
		if (!diskPath)
		{
			KOI_LOG_ERROR("Cannot resolve scene path to disk for saving: {}", virtualPath);
			return false;
		}

		std::ofstream out(*diskPath, std::ios::trunc);
		if (!out)
		{
			KOI_LOG_ERROR("Failed to open scene file for writing: {}", diskPath->string());
			return false;
		}

		out << root;
		KOI_LOG_INFO("Saved scene '{}'", diskPath->string());
		return true;
	}

	toml::table World::SaveToTable()
	{
		ZoneScoped;

		// Build the document in the same shape LoadFromTable reads
		toml::table root;
		toml::array entities;

		const std::span<const ComponentInfo> components = ComponentRegistry::Get().All();
		m_ecs.query<const Transform>().each([&](Entity e, const Transform&)
		{
			if (e == m_root) return;  // skip the internal WorldRoot

			toml::table entity;

			// Save name
			entity.insert("name", std::string(e.name().c_str()));

			// Save parent
			if (const Entity parent = e.parent(); parent && parent != m_root)
				entity.insert("parent", std::string(parent.name().c_str()));

			// Save components (use registry for this)
			for (const ComponentInfo& component : components)
				component.Save(e, entity, component.Key);

			entities.push_back(std::move(entity));
		});

		root.insert("entity", std::move(entities));
		return root;
	}

	void World::Clear(GfxDevice& device)
	{
		ZoneScoped;

		// Give back every asset reference the entities took when they were loaded
		AssetCache<GPUMesh>& meshes = Engine::Get().GetAssets().Cache<GPUMesh>();
		m_ecs.each<MeshRef>([&](Entity, MeshRef& mesh)
		{
			meshes.Release(device, mesh.Handle);
		});

		// Collect first to prevent iterator invalidation
		std::vector<EntityID> doomed;
		m_ecs.each<Transform>([&](Entity e, Transform&)
		{
			if (e != m_root) doomed.push_back(e.id());
		});

		for (const EntityID id : doomed)
		{
			if (const Entity e = m_ecs.entity(id); e.is_alive())
				e.destruct();
		}

		m_bounds.Min = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		m_bounds.Max = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	}

	void World::Update(f32 dt)
	{
		ZoneScoped;
		// NOTE: Might refactor the entire world update system
		if (!Engine::Get().IsPlaying()) return;

		m_ecs.progress(dt);
	}

	const Camera* World::ResolveCamera(f32 aspect)
	{
		ZoneScoped;

		bool found = false;
		m_ecs.query<const Transform, const CameraComponent>()
			.each([&](const Transform& xform, const CameraComponent& cam)
			{
				if (!cam.Primary || found) return;
				m_camera.SetProjection(cam.FovY, aspect, cam.Near, cam.Far);
				m_camera.SetTransform(xform);
				found = true;
			});

		return found ? &m_camera : nullptr;
	}

	const GPUMesh* World::GetMesh(MeshHandle handle) const
	{
		return Engine::Get().GetAssets().Cache<GPUMesh>().Get(handle);
	}
}
