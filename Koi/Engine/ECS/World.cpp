#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Math/TransformSerializer.h"
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

		m_ecs.system<const Transform, const WorldTransform*, WorldTransform>("UpdateWorldTransforms")
			.term_at(1).parent().cascade()
			.kind(flecs::PostUpdate)
			.each([](const Transform& local, const WorldTransform* parentWorld, WorldTransform& worldXform)
			{
				const Matrix localMat = local.ToMatrix();
				worldXform.Value = parentWorld ? (localMat * parentWorld->Value) : localMat;
			});

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

			const toml::array* entities = scene["entity"].as_array();
			if (!entities)
			{
				KOI_LOG_WARN("Scene '{}' has no [[entity]] array; loading empty world", virtualPath);
				return world;
			}

			// Pass 1: create each entity + its components, deferring parent links (by name)
			// so an entity can reference a parent declared later in the file
			struct PendingParent { Entity Child; std::string ParentName; };
			std::vector<PendingParent> pendingParents;

			for (const toml::node& node : *entities)
			{
				const toml::table* entityTable = node.as_table();
				if (!entityTable) continue;

				const std::string name = (*entityTable)["name"].value_or(std::string("Entity"));
				Entity e = world.m_ecs.entity(name.c_str());

				// Transform (identity when the sub-table is absent)
				const Transform transform = DeserializeOr<Transform>(*entityTable, "transform", Transform{});
				e.set<Transform>(transform).set<WorldTransform>({});

				// Optional mesh component: [entity.mesh] path = "project://..."
				if (const toml::table* meshTable = (*entityTable)["mesh"].as_table())
				{
					const std::string meshPath = (*meshTable)["path"].value_or(std::string());
					if (!meshPath.empty())
					{
						const MeshHandle handle = world.m_meshRegistry.GetOrLoad(meshPath, device, &world.m_bounds);
						e.set<MeshRef>({ meshPath, handle });
					}
				}

				if (const std::string parentName = (*entityTable)["parent"].value_or(std::string()); !parentName.empty())
					pendingParents.push_back({ e, parentName });
			}

			// Pass 2: resolve parent links now that every named entity exists
			for (const PendingParent& link : pendingParents)
			{
				if (const Entity parent = world.m_ecs.lookup(link.ParentName.c_str()))
					link.Child.child_of(parent);
				else
					KOI_LOG_WARN("Scene entity parent '{}' not found", link.ParentName);
			}

			// Frame the camera on whatever geometry loaded (empty scenes get a default view)
			world.m_camera.SetProjection(0.8f, 16.0f / 9.0f, 0.05f, 1000.0f);
			if (world.m_bounds.Min.x <= world.m_bounds.Max.x)
			{
				const Vector3 center = world.m_bounds.Center();
				const f32 radius = world.m_bounds.Radius();
				world.m_camera.SetView(center + Vector3(0.0f, 0.0f, radius * 2.5f), center);
			}
			else
			{
				world.m_camera.SetView(Vector3(0.0f, 1.0f, 5.0f), Vector3::Zero);
			}

			KOI_LOG_INFO("Loaded scene '{}' ({} entities)", virtualPath, entities->size());
			return world;
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_ERROR("Failed to parse scene '{}': {}", virtualPath, e.description());
			return std::nullopt;
		}
	}

	bool World::Save(const fs::path& path)
	{
		ZoneScoped;

		const std::string virtualPath = "project://" + path.generic_string();

		// Build the document in the same shape World::Load reads
		toml::table root;
		toml::array entities;

		m_ecs.query<const Transform>().each([&](Entity e, const Transform& transform)
		{
			if (e == m_root) return;  // skip the internal WorldRoot

			toml::table entity;
			entity.insert("name", std::string(e.name().c_str()));

			if (const Entity parent = e.parent(); parent && parent != m_root)
				entity.insert("parent", std::string(parent.name().c_str()));

			entity.insert("transform", Serializer<Transform>::Serialize(transform));

			if (e.has<MeshRef>())
			{
				const MeshRef& mesh = e.get<MeshRef>();
				toml::table meshTable;
				meshTable.insert("path", mesh.SourcePath);
				entity.insert("mesh", std::move(meshTable));
			}

			entities.push_back(std::move(entity));
		});

		root.insert("entity", std::move(entities));

		// project:// must resolve to a real file (editor source tree) to write back.
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

	void World::Update(f32 dt)
	{
		ZoneScoped;
		m_ecs.progress(dt);
	}

	void World::Release(GfxDevice& device)
	{
		ZoneScoped;
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		m_meshRegistry.Release(device);
	}
}
