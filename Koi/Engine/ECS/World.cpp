#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <cassert>
#include <fstream>

namespace koi
{
	World::World()
	{
		ZoneScoped;

		m_root = m_ecs.entity("WorldRoot");

		m_ecs.set_ctx(this);
		m_fixedPipeline = m_ecs.pipeline().with(flecs::System).with<internal::FixedUpdateTag>().build();

		ConfigureComponentHooks();
		BuildSystems();
	}

	World::~World() = default;

	World::World(World&& other) noexcept
		: m_ecs(std::move(other.m_ecs))
		, m_root(other.m_root)
		, m_fixedPipeline(other.m_fixedPipeline)
		, m_fixedAccumulator(other.m_fixedAccumulator)
		, m_systems(std::move(other.m_systems))
		, m_sourcePath(std::move(other.m_sourcePath))
		, m_bounds(other.m_bounds)
		, m_camera(other.m_camera)
	{
		// System callbacks find their World through the flecs context, so it must follow the move
		if (m_ecs.c_ptr()) m_ecs.set_ctx(this);
	}

	World& World::operator=(World&& other) noexcept
	{
		if (this == &other) return *this;

		m_ecs              = std::move(other.m_ecs);
		m_root             = other.m_root;
		m_fixedPipeline    = other.m_fixedPipeline;
		m_fixedAccumulator = other.m_fixedAccumulator;
		m_systems          = std::move(other.m_systems);
		m_sourcePath       = std::move(other.m_sourcePath);
		m_bounds           = other.m_bounds;
		m_camera           = other.m_camera;

		if (m_ecs.c_ptr()) m_ecs.set_ctx(this);
		return *this;
	}

	World& World::From(const flecs::world& ecs)
	{
		World* world = static_cast<World*>(ecs.get_ctx());
		assert(world && "flecs world is not owned by a koi::World");
		return *world;
	}

	std::pair<u64, u64> World::OrderKey(Entity e)
	{
		const SceneOrder* order = e.try_get<SceneOrder>();
		return { order ? order->Index : ~0ull, e.id() };
	}

	EntityBuilder World::Spawn(std::string_view name)
	{
		Entity e = name.empty() ? m_ecs.entity() : m_ecs.entity(std::string(name).c_str());
		e.set<Transform>({});
		return EntityBuilder(e);
	}

	void World::BuildSystems()
	{
		m_systems = SystemRegistry::Get().Instantiate();
		for (const std::unique_ptr<ISystem>& system : m_systems)
		{
			system->Build(*this);
			KOI_LOG_TRACE("Built system '{}'", system->Name());
		}
	}

	void World::ConfigureComponentHooks()
	{
		// Whatever drops a MeshRef - a removed component, a destroyed entity, a torn-down
		// world - gives the cache reference back here, so no call site has to remember to
		m_ecs.component<MeshRef>().on_remove([](Entity, MeshRef& mesh)
		{
			if (!mesh.Handle) return;

			Engine& engine = Engine::Get();
			engine.GetSubsystem<AssetRegistry>().Cache<GPUMesh>()
				.Release(engine.GetSubsystem<Renderer>().GetDevice(), mesh.Handle);
		});

		m_ecs.component<MaterialRef>().on_remove([](Entity, MaterialRef& material)
		{
			if (!material.Handle) return;

			Engine& engine = Engine::Get();
			engine.GetSubsystem<AssetRegistry>().Cache<Material>()
				.Release(engine.GetSubsystem<Renderer>().GetDevice(), material.Handle);
		});
	}

	std::optional<World> World::Load(std::string_view virtualPath)
	{
		ZoneScoped;
		KOI_LOG_DEBUG("Loading scene '{}'", virtualPath);

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
			world.m_sourcePath = virtualPath;
			world.LoadFromTable(scene);

			KOI_LOG_INFO("Loaded scene '{}'", virtualPath);
			return world;
		}
		catch (const toml::parse_error& e)
		{
			KOI_LOG_ERROR("Failed to parse scene '{}': {}", virtualPath, e.description());
			return std::nullopt;
		}
	}

	bool World::LoadFromTable(const toml::table& scene)
	{
		ZoneScoped;

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

		const auto components = ComponentRegistry::Get().All();
		u32 index = 0;
		for (const toml::node& node : *entities)
		{
			const u32 i = index++;

			const toml::table* entityTable = node.as_table();
			if (!entityTable) continue;

			// Create entity
			const std::string name = (*entityTable)["name"].value_or<std::string>("Entity_" + std::to_string(i));
			if (m_ecs.lookup(name.c_str()))
			{
				KOI_LOG_WARN("Scene entity [{}] '{}' duplicates an earlier name. Flecs will merge them into one entity", i, name);
			}

			Entity e = m_ecs.entity(name.c_str());
			e.set<SceneOrder>({ i });
			KOI_LOG_TRACE("Scene entity [{}] '{}'", i, name);

			// Load components (use registry for this)
			for (const std::unique_ptr<IComponentType>& component : components)
			{
				component->Load(e, *entityTable);
			}

			// A sub-table no component claims is usually a renamed key
			for (const auto& [key, value] : *entityTable)
			{
				if (!value.is_table() || ComponentRegistry::Get().Find(key.str())) continue;
				KOI_LOG_WARN("Scene entity '{}' has unknown component table '{}'; it will not be loaded or saved", name, key.str());
			}

			// Configure parents if they exist
			if (const std::string parentName = (*entityTable)["parent"].value_or(std::string()); !parentName.empty())
				pendingParents.push_back({ e, parentName });
		}

		// Pass 2: resolve parent links now that every named entity exists
		for (const PendingParent& link : pendingParents)
		{
			if (const Entity parent = m_ecs.lookup(link.ParentName.c_str()))
			{
				link.Child.child_of(parent);
			}
			else
			{
				KOI_LOG_WARN("Scene entity parent '{}' not found", link.ParentName);
			}
		}

		// World bounds come from whichever meshes ended up resident
		const AssetCache<GPUMesh>& meshes = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();
		m_ecs.each<MeshRef>([&](Entity, MeshRef& mesh)
		{
			if (const GPUMesh* gpuMesh = meshes.Get(mesh.Handle))
			{
				math::MergeBounds(m_bounds, gpuMesh->Bounds);
			}
		});

		// If no deserialized camera exists, create one centered on the scene world bounds
		bool hasCamera = false;
		m_ecs.query<const CameraComponent>().each([&](const CameraComponent&) { hasCamera = true; });
		if (!hasCamera)
		{
			Transform camXform;
			if (!math::IsEmpty(m_bounds))
			{
				const Vector3 center(m_bounds.Center);
				const f32 radius  = math::BoundsRadius(m_bounds);
				camXform.Position = center + Vector3(0.0f, 0.0f, radius * 2.5f);
			}
			else
			{
				camXform.Position = Vector3(0.0f, 1.0f, 5.0f);
			}

			m_ecs.entity("Camera")
				.set<Transform>(camXform)
				.set<CameraComponent>({});

			KOI_LOG_DEBUG("Scene has no camera; spawned a default one");
		}

		KOI_LOG_DEBUG("Built world from scene table ({} entities)", entities->size());
		return true;
	}

	bool World::Save()
	{
		ZoneScoped;

		if (m_sourcePath.empty())
		{
			KOI_LOG_ERROR("World was not loaded from a scene; nothing to save to");
			return false;
		}

		const toml::table root = SaveToTable();

		const std::optional<fs::path> diskPath = VirtualFileSystem::Get().TryResolveToDisk(m_sourcePath);
		if (!diskPath)
		{
			KOI_LOG_ERROR("Cannot resolve scene path to disk for saving: {}", m_sourcePath);
			return false;
		}

		std::ofstream out(*diskPath, std::ios::trunc);
		if (!out)
		{
			KOI_LOG_ERROR("Failed to open scene file for writing: {}", diskPath->string());
			return false;
		}

		out << utils::TomlDocument(root);
		KOI_LOG_INFO("Saved scene '{}'", diskPath->string());
		return true;
	}

	toml::table World::SaveToTable()
	{
		ZoneScoped;

		// Build the document in the same shape LoadFromTable reads
		toml::table root;
		toml::array entities;

		// File order follows SceneOrder
		std::vector<Entity> ordered;
		m_ecs.query<const Transform>().each([&](Entity e, const Transform&)
		{
			if (e != m_root) ordered.push_back(e);
		});
		std::ranges::stable_sort(ordered, {}, [](Entity e) { return OrderKey(e); });

		const auto components = ComponentRegistry::Get().All();
		for (const Entity e : ordered)
		{
			toml::table entity;

			// Save name
			entity.insert("name", std::string(e.name().c_str()));

			// Save parent
			if (const Entity parent = e.parent(); parent && parent != m_root)
				entity.insert("parent", std::string(parent.name().c_str()));

			// Save components (use registry for this)
			for (const std::unique_ptr<IComponentType>& component : components)
			{
				component->Save(e, entity);
			}

			entities.push_back(std::move(entity));
		}

		root.insert("entity", std::move(entities));
		KOI_LOG_TRACE("Serialized world to scene table ({} entities)", root["entity"].as_array()->size());
		return root;
	}

	void World::Clear()
	{
		ZoneScoped;

		// Destructing an entity fires the component hooks, which is what releases its assets

		// Collect first to prevent iterator invalidation
		std::vector<EntityID> doomed;
		m_ecs.each<Transform>([&](Entity e, Transform&)
		{
			if (e != m_root) doomed.push_back(e.id());
		});

		for (const EntityID id : doomed)
		{
			if (const Entity e = m_ecs.entity(id); e.is_alive())
			{
				e.destruct();
			}
		}

		KOI_LOG_DEBUG("Cleared world ({} entities destroyed)", doomed.size());

		m_bounds = BoundingBox{ {}, {} };
	}

	void World::Update(f32 dt)
	{
		ZoneScoped;
		if (!Engine::Get().IsPlaying()) return;

		// Timer already clamps dt, which bounds how many fixed steps one frame can owe
		m_fixedAccumulator += dt;
		while (m_fixedAccumulator >= kFixedStep)
		{
			m_ecs.run_pipeline(m_fixedPipeline, kFixedStep);
			m_fixedAccumulator -= kFixedStep;
		}

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
		return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().Get(handle);
	}
}
