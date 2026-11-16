#include "Koi/Engine/ECS/World.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Core/Assert.h"
#include "Koi/Engine/Core/Exception.h"
#include "Koi/Engine/Diagnostics/Profiling.h"
#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/ECS/EntityBuilder.h"
#include "Koi/Engine/ECS/ObserverRegistry.h"
#include "Koi/Engine/ECS/SystemRegistry.h"
#include "Koi/Engine/Graphics/RenderScene.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/IO/VirtualFileSystem.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Math.h"
#include "Koi/Engine/Runtime/Engine.h"
#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace koi
{
	World::World()
	{
		ZoneScoped;

		m_ecs         = std::make_unique<ECS>(*this);
		m_renderScene = std::make_unique<RenderScene>(*m_ecs);

		ConfigureComponentHooks();
		BuildSystems();
		BuildObservers();
	}

	World::~World() = default;

	std::pair<u64, u64> World::OrderKey(Entity e)
	{
		const SceneOrder* order = e.TryGet<const SceneOrder>();
		return { order ? order->Index : ~0ull, e.GetId() };
	}

	EntityBuilder World::Spawn(std::string_view name)
	{
		const Entity e = m_ecs->Spawn(name);
		e.Set<Transform>({});
		return EntityBuilder(e);
	}

	void World::BuildSystems()
	{
		m_systems = SystemRegistry::Get().Instantiate();
		for (const std::unique_ptr<ISystem>& system : m_systems)
		{
			system->Build(*this);
			KOI_LOG(ECS, Trace, "Built system '{}'", system->Name());
		}
	}

	void World::BuildObservers()
	{
		m_observers = ObserverRegistry::Get().Instantiate();
		for (const std::unique_ptr<IObserver>& observer : m_observers)
		{
			observer->Build(*this);
			KOI_LOG(ECS, Trace, "Built observer '{}'", observer->Name());
		}
	}

	void World::ConfigureComponentHooks()
	{
		// Whatever drops a MeshRef - a removed component, a destroyed entity, a torn-down
		// world - gives the cache reference back here, so no call site has to remember to
		m_ecs->OnRemove<MeshRef>([](Entity, MeshRef& mesh)
		{
			if (!mesh.Handle) return;

			Engine& engine = Engine::Get();
			engine.GetSubsystem<AssetRegistry>().Cache<GPUMesh>()
				.Release(engine.GetSubsystem<Renderer>().GetDevice(), mesh.Handle);
		});

		m_ecs->OnRemove<MaterialRef>([](Entity, MaterialRef& material)
		{
			if (!material.Handle) return;

			Engine& engine = Engine::Get();
			engine.GetSubsystem<AssetRegistry>().Cache<Material>()
				.Release(engine.GetSubsystem<Renderer>().GetDevice(), material.Handle);
		});
	}

	Result<void> World::Load(std::string_view virtualPath)
	{
		ZoneScoped;
		KOI_LOG(Scene, Debug, "Loading scene '{}'", virtualPath);

		const Result<std::vector<byte>> bytes = VirtualFileSystem::Get().Read(virtualPath);
		if (!bytes)
		{
			return std::unexpected(bytes.error());
		}

		const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());

		// Parsed before anything is torn down, so a broken file leaves the current scene alone
		toml::table scene;
		try
		{
			scene = toml::parse(text);
		}
		catch (const toml::parse_error& e)
		{
			return Error::Make("line {}: {}", e.source().begin.line, e.description());
		}

		Load(scene, virtualPath);
		KOI_LOG(Scene, Info, "Loaded scene '{}'", virtualPath);
		return {};
	}

	void World::Load(const toml::table& scene, std::string_view sourcePath)
	{
		const bool inPlay = Engine::Get().GetPlayState() != PlayState::Edit;
		if (inPlay) EndPlay();

		Clear();
		m_sourcePath = sourcePath;
		LoadFromTable(scene);

		if (inPlay) BeginPlay();
	}

	void World::LoadFromTable(const toml::table& scene)
	{
		ZoneScoped;

		const toml::array* entities = scene["entity"].as_array();
		if (!entities)
		{
			KOI_LOG(Scene, Warn, "Scene has no [[entity]] array; loading empty world");
			return;
		}

		// Pass 1: create each entity + its components, deferring parent links (by id)
		struct PendingParent { Entity Child; i64 ParentId; };
		std::vector<PendingParent> pendingParents;
		std::unordered_map<i64, Entity> byId;

		const auto components = ComponentRegistry::Get().All();
		u32 index = 0;
		for (const toml::node& node : *entities)
		{
			const u32 i = index++;

			const toml::table* entityTable = node.as_table();
			if (!entityTable) continue;

			// Create entity
			const std::string name = (*entityTable)["name"].value_or<std::string>("Entity_" + std::to_string(i));
			const Entity e = m_ecs->Spawn(name);

			// Ids default to the entity's position in the file
			const i64 id = (*entityTable)["id"].value_or(static_cast<i64>(i));
			if (!byId.try_emplace(id, e).second)
			{
				KOI_LOG(Scene, Warn, "Scene entity [{}] '{}' reuses id {}; children of that id attach to the first", i, name, id);
			}

			e.Set<SceneOrder>({ i });

			// Load components (use registry for this)
			for (const std::unique_ptr<IComponentType>& component : components)
			{
				component->Load(e, *entityTable);
			}

			// A sub-table no component claims is usually a renamed key
			for (const auto& [key, value] : *entityTable)
			{
				if (!value.is_table() || ComponentRegistry::Get().Find(key.str())) continue;
				KOI_LOG(Scene, Warn, "Scene entity '{}' has unknown component table '{}'; it will not be loaded or saved", name, key.str());
			}

			// Configure parents if they exist
			if (const std::optional<i64> parentId = (*entityTable)["parent"].value<i64>())
			{
				pendingParents.push_back({ e, *parentId });
			}
			else if ((*entityTable).contains("parent"))
			{
				KOI_LOG(Scene, Warn, "Scene entity '{}' has a non-integer parent; it should be the parent's id", name);
			}
		}

		// Pass 2: resolve parent links now that every entity exists
		for (const PendingParent& link : pendingParents)
		{
			const auto parent = byId.find(link.ParentId);
			if (parent == byId.end())
			{
				KOI_LOG(Scene, Warn, "Scene entity '{}' has parent id {}, which doesn't exist", link.Child.GetName(), link.ParentId);
			}
			else if (parent->second == link.Child || parent->second.IsDescendantOf(link.Child))
			{
				KOI_LOG(Scene, Warn, "Scene entity '{}' would parent into its own subtree; left at the top level", link.Child.GetName());
			}
			else
			{
				link.Child.SetParent(parent->second);
			}
		}

		// World bounds come from whichever meshes ended up resident
		const AssetCache<GPUMesh>& meshes = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>();
		m_ecs->Each<const MeshRef>([&](const MeshRef& mesh)
		{
			if (const GPUMesh* gpuMesh = meshes.Get(mesh.Handle))
			{
				math::MergeBounds(m_bounds, gpuMesh->Bounds);
			}
		});

		// If no deserialized camera exists, create one centered on the scene world bounds
		bool hasCamera = false;
		m_ecs->Each<const CameraComponent>([&](const CameraComponent&) { hasCamera = true; });
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

			m_ecs->Spawn("Camera")
				.Set<Transform>(camXform)
				.Set<CameraComponent>({});

			KOI_LOG(Scene, Debug, "Scene has no camera; spawned a default one");
		}

		PropagateTransforms();
		KOI_LOG(Scene, Debug, "Built world from scene table ({} entities)", entities->size());
	}

	Result<void> World::Save()
	{
		ZoneScoped;

		if (m_sourcePath.empty())
		{
			return Error::Make("the world wasn't loaded from a scene, so there's nowhere to save it");
		}

		const toml::table root = SaveToTable();

		const std::optional<fs::path> diskPath = VirtualFileSystem::Get().TryResolveToDisk(m_sourcePath);
		if (!diskPath)
		{
			return Error::Make("{} doesn't resolve to a file on disk", m_sourcePath);
		}

		std::ofstream out(*diskPath, std::ios::trunc);
		if (!(out << utils::TomlDocument(root)))
		{
			return Error::Make("couldn't write {}", diskPath->string());
		}

		KOI_LOG(Scene, Info, "Saved scene '{}'", diskPath->string());
		return {};
	}

	toml::table World::SaveToTable()
	{
		ZoneScoped;

		// Build the document in the same shape LoadFromTable reads
		toml::table root;
		toml::array entities;

		// File order follows SceneOrder
		std::vector<Entity> ordered;
		m_ecs->Each<const Transform>([&](Entity e, const Transform&)
		{
			ordered.push_back(e);
		});
		std::ranges::stable_sort(ordered, {}, [](Entity e) { return OrderKey(e); });

		// An entity's id is its position in the file, so names never have to be unique
		std::unordered_map<EntityID, i64> ids;
		for (const Entity e : ordered)
		{
			ids.emplace(e.GetId(), static_cast<i64>(ids.size()));
		}

		const auto components = ComponentRegistry::Get().All();
		for (const Entity e : ordered)
		{
			toml::table entity;

			entity.insert("id", ids.at(e.GetId()));
			entity.insert("name", std::string(e.GetName()));

			if (const Entity parent = e.GetParent(); parent)
			{
				if (const auto it = ids.find(parent.GetId()); it != ids.end())
				{
					entity.insert("parent", it->second);
				}
			}

			// Save components (use registry for this)
			for (const std::unique_ptr<IComponentType>& component : components)
			{
				component->Save(e, entity);
			}

			entities.push_back(std::move(entity));
		}

		root.insert("entity", std::move(entities));
		KOI_LOG(Scene, Trace, "Serialized world to scene table ({} entities)", root["entity"].as_array()->size());
		return root;
	}

	void World::Clear()
	{
		ZoneScoped;

		// Destructing an entity fires the component hooks, which is what releases its assets
		m_ecs->Clear();
		KOI_LOG(Scene, Debug, "Cleared world");

		m_bounds = BoundingBox{ {}, {} };
	}

	void World::Update(f32 dt)
	{
		ZoneScoped;
		if (!Engine::Get().IsPlaying())
		{
			m_ecs->Sync();
			PropagateTransforms();
			return;
		}

		// Timer already clamps dt, which bounds how many fixed steps one frame can owe
		m_fixedAccumulator += dt;
		{
			KOI_SCOPE("Fixed Update");
			i64 steps = 0;
			while (m_fixedAccumulator >= kFixedStep && !m_systemFailure)
			{
				ZoneScopedN("Fixed Step");
				m_ecs->RunPhases(true, kFixedStep);
				m_fixedAccumulator -= kFixedStep;
				++steps;
			}
			FrameStats::Count("Fixed Steps", steps);
			if (steps > 0) PropagateTransforms();
		}

		if (!m_systemFailure)
		{
			KOI_SCOPE("Frame Systems");
			m_ecs->RunPhases(false, dt);
			PropagateTransforms();
		}

		if (m_systemFailure)
		{
			SystemFailure failure = *std::exchange(m_systemFailure, std::nullopt);
			throw Exception(failure.Message, std::move(failure.Trace));
		}
	}

	void World::PropagateTransforms()
	{
		KOI_SCOPE("Propagate Transforms");

		// Collected first: a missing WorldTransform is added on the way, which defers inside a query
		std::vector<Entity> roots;
		m_ecs->Each<const Transform>([&](Entity e, const Transform&)
		{
			if (!e.GetParent()) roots.push_back(e);
		});

		i64 visited = 0;
		const auto visit = [&](this const auto& self, Entity e, const WorldTransform* parent) -> void
		{
			// Copied, since adding a WorldTransform further down can move the pool
			std::optional<WorldTransform> mine;
			if (const Transform* local = e.TryGet<const Transform>())
			{
				mine = WorldTransform::Compose(*local, parent);
				e.Set<WorldTransform>(*mine);
				++visited;
			}

			const WorldTransform* passDown = mine ? &*mine : parent;
			for (const Entity child : e.Children())
			{
				self(child, passDown);
			}
		};

		for (const Entity root : roots)
		{
			visit(root, nullptr);
		}
		FrameStats::Count("Transforms", visited);

		m_renderScene->Sync();
	}

	void World::Reparent(Entity child, Entity parent)
	{
		const WorldTransform* world = child.TryGet<const WorldTransform>();
		const std::optional<Matrix> worldMatrix = world ? std::optional(world->World) : std::nullopt;

		child.SetParent(parent);

		Transform* local = child.TryGet<Transform>();
		if (!local || !worldMatrix) return;

		const WorldTransform* parentWorld = parent ? parent.TryGet<const WorldTransform>() : nullptr;
		*local = WorldTransform::ToLocal(*worldMatrix, parentWorld);
	}

	void World::BeginPlay()
	{
		ZoneScoped;
		m_fixedAccumulator = 0.0f;
		for (const std::unique_ptr<ISystem>& system : m_systems)
		{
			system->OnBeginPlay(*this);
		}
		for (const std::unique_ptr<IObserver>& observer : m_observers)
		{
			observer->OnBeginPlay(*this);
		}

		m_ecs->SetPlaying(true);
		m_ecs->EmitToAll(koi::BeginPlay{});
		m_ecs->Sync();
	}

	void World::EndPlay()
	{
		ZoneScoped;
		m_ecs->EmitToAll(koi::EndPlay{});
		m_ecs->Sync();
		m_ecs->SetPlaying(false);

		for (auto it = m_observers.rbegin(); it != m_observers.rend(); ++it)
		{
			(*it)->OnEndPlay(*this);
		}
		for (auto it = m_systems.rbegin(); it != m_systems.rend(); ++it)
		{
			(*it)->OnEndPlay(*this);
		}
	}

	void World::ReportSystemFailure(std::string_view system, std::string_view what, std::stacktrace trace)
	{
		if (m_systemFailure) return;
		m_systemFailure = SystemFailure{ std::format("System '{}' threw: {}", system, what), std::move(trace) };
	}

	const PostProcess& World::ResolvePostProcess() const
	{
		const PostProcess* found = nullptr;
		m_ecs->Each<const PostProcess>([&](const PostProcess& post)
		{
			if (!found) found = &post;
		});
		static const PostProcess kDefault{};
		return found ? *found : kDefault;
	}

	const Camera* World::ResolveCamera(f32 aspect)
	{
		ZoneScoped;

		bool found = false;
		m_ecs->Query<const WorldTransform, const CameraComponent>()
			.Each([&](const WorldTransform& xform, const CameraComponent& cam)
			{
				if (!cam.Primary || found) return;
				m_camera.SetProjection(cam.FovY, aspect, cam.Near, cam.Far);
				m_camera.SetTransform(xform.AsTransform());
				found = true;
			});

		return found ? &m_camera : nullptr;
	}

	const GPUMesh* World::GetMesh(MeshHandle handle) const
	{
		return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().Get(handle);
	}
}
