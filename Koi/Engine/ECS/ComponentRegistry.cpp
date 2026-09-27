#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Runtime/ReflectSerializer.h"
#include <algorithm>

namespace koi
{
	namespace
	{
		AssetCache<GPUMesh>&  Meshes()    { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>(); }
		AssetCache<Material>& Materials() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<Material>(); }
		GfxDevice&            Device()    { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }

		constexpr std::string_view kDefaultMesh     = "engine://Content/Meshes/Box.gltf";
		constexpr std::string_view kDefaultMaterial = "engine://Content/Materials/Default.mat.toml";

		// MeshRef is a cache handle rather than plain data, so it cannot depend on Serializer<T>
		class MeshComponentType final : public IComponentType
		{
		public:
			[[nodiscard]] std::string_view Key() const override { return MeshRef::kKey; }
			[[nodiscard]] i32 DrawOrder() const override { return 10; }

			[[nodiscard]] bool Has(Entity e) const override { return e.has<MeshRef>(); }

			// Default to a box so a freshly added component has something visible
			void Add(Entity e) const override
			{
				const MeshHandle handle = Meshes().Acquire(Device(), AssetKey(std::string(kDefaultMesh)));
				if (handle) e.set<MeshRef>({ handle });
			}

			// World's MeshRef on_remove hook hands the cache reference back
			void Remove(Entity e) const override { e.remove<MeshRef>(); }

			void Load(Entity e, const toml::table& entityTable) const override
			{
				const toml::table* meshTable = entityTable[Key()].as_table();
				if (!meshTable) return;

				const AssetKey key((*meshTable)["path"].value_or(std::string()));
				if (!key) return;

				const MeshHandle handle = Meshes().Acquire(Device(), key);
				if (handle) e.set<MeshRef>({ handle });
			}

			void Save(Entity e, toml::table& entityTable) const override
			{
				const MeshRef* meshRef = e.try_get<MeshRef>();
				if (!meshRef) return;

				toml::table meshTable;
				meshTable.insert("path", Meshes().GetKey(meshRef->Handle).Path());
				entityTable.insert(Key(), std::move(meshTable));
			}
		};

		// Same shape as MeshComponentType: a cache handle, keyed by path in the scene file
		class MaterialComponentType final : public IComponentType
		{
		public:
			[[nodiscard]] std::string_view Key() const override { return MaterialRef::kKey; }
			[[nodiscard]] i32 DrawOrder() const override { return 11; }

			[[nodiscard]] bool Has(Entity e) const override { return e.has<MaterialRef>(); }

			void Add(Entity e) const override
			{
				const MaterialHandle handle = Materials().Acquire(Device(), AssetKey(std::string(kDefaultMaterial)));
				if (handle) e.set<MaterialRef>({ handle });
			}

			void Remove(Entity e) const override { e.remove<MaterialRef>(); }

			void Load(Entity e, const toml::table& entityTable) const override
			{
				const toml::table* table = entityTable[Key()].as_table();
				if (!table) return;

				const AssetKey key((*table)["path"].value_or(std::string()));
				if (!key) return;

				const MaterialHandle handle = Materials().Acquire(Device(), key);
				if (handle) e.set<MaterialRef>({ handle });
			}

			void Save(Entity e, toml::table& entityTable) const override
			{
				const MaterialRef* ref = e.try_get<MaterialRef>();
				if (!ref) return;

				toml::table table;
				table.insert("path", Materials().GetKey(ref->Handle).Path());
				entityTable.insert(Key(), std::move(table));
			}
		};
	}

	ComponentRegistry& ComponentRegistry::Get()
	{
		static ComponentRegistry s_instance;
		return s_instance;
	}

	void ComponentRegistry::Register(std::unique_ptr<IComponentType> type)
	{
		if (!type) return;

		if (Find(type->Key()))
		{
			KOI_LOG_ERROR("Component '{}' is already registered; ignoring the second registration", type->Key());
			return;
		}

		KOI_LOG_DEBUG("Registered component '{}' (draw order {})", type->Key(), type->DrawOrder());

		const auto at = std::ranges::upper_bound(m_types, type->DrawOrder(), {},
			[](const std::unique_ptr<IComponentType>& t) { return t->DrawOrder(); });

		m_types.insert(at, std::move(type));
	}

	const IComponentType* ComponentRegistry::Find(std::string_view key) const
	{
		const auto it = std::ranges::find_if(m_types,
			[key](const std::unique_ptr<IComponentType>& t) { return t->Key() == key; });

		return it != m_types.end() ? it->get() : nullptr;
	}

	void RegisterEngineComponents()
	{
		ComponentRegistry& registry = ComponentRegistry::Get();

		registry.Register<ComponentType<Transform>>(0, true);
		registry.Register<MeshComponentType>();
		registry.Register<MaterialComponentType>();
		registry.Register<ComponentType<CameraComponent>>(20);
		registry.Register<ComponentType<Light>>(30);
	}
}
