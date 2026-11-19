#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/ComponentSerializers.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Core/UUID.h"
#include "Koi/Engine/ECS/Components.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/Transform.h"
#include "Koi/Engine/Physics/PhysicsComponents.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Core/ReflectSerializer.h"
#include <algorithm>

namespace koi
{
	namespace
	{
		template<typename T>
		AssetCache<T>& Cache() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<T>(); }
		GfxDevice&     Device() { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }

		constexpr std::string_view kDefaultMesh     = "engine://Content/Meshes/Box.kmesh";
		constexpr std::string_view kDefaultMaterial = "engine://Content/Materials/Default.kmat";

		// Scenes name binary assets by id, so moving or renaming the file doesn't break them. A missing
		// asset still hands back an (empty) key, so the entity shows the fallback instead of vanishing
		AssetKey ReadAssetRef(const toml::table& table, std::string_view what)
		{
			if (const std::optional<std::string> id = table["id"].value<std::string>())
			{
				if (const AssetIndexEntry* entry = Engine::Get().GetSubsystem<AssetRegistry>().Index().Find(utils::UUIDFromString(*id)))
					return AssetKey(entry->VirtualPath);

				KOI_LOG(Scene, Warn, "The scene wants {} {}, which isn't in the project anymore. Showing the fallback", what, *id);
				return AssetKey();
			}

			// Hand-written scenes may still name the file
			return AssetKey(table["path"].value_or(std::string()));
		}

		void WriteAssetRef(toml::table& table, const AssetKey& key)
		{
			if (const AssetIndexEntry* entry = Engine::Get().GetSubsystem<AssetRegistry>().Index().FindByPath(key.Path()))
				table.insert("id", utils::ToString(entry->Id));
			else
				table.insert("path", key.Path());  // generated or unindexed, so the path is all there is
		}

		// A cache handle named by id in the file
		template<typename Ref, typename T>
		class AssetRefComponentType final : public IComponentType
		{
		public:
			AssetRefComponentType(i32 drawOrder, std::string_view defaultPath) : m_drawOrder(drawOrder), m_defaultPath(defaultPath) {}

			[[nodiscard]] std::string_view Key() const override { return Ref::kKey; }
			[[nodiscard]] std::string_view Category() const override { return Ref::kCategory; }
			[[nodiscard]] i32 DrawOrder() const override { return m_drawOrder; }

			[[nodiscard]] bool Has(Entity e) const override { return e.Has<Ref>(); }

			void Add(Entity e) const override
			{
				const AssetHandle<T> handle = Cache<T>().Acquire(Device(), AssetKey(std::string(m_defaultPath)));
				if (handle) e.Set<Ref>({ handle });
			}

			// World's OnRemove hook hands the cache reference back
			void Remove(Entity e) const override { e.Remove<Ref>(); }

			void Load(Entity e, const toml::table& entityTable) const override
			{
				const toml::node* node = entityTable.get(Key());
				if (!node) return;

				if (const std::optional<Ref> ref = Serializer<Ref>::Deserialize(*node); ref && ref->Handle)
				{
					e.Set<Ref>(*ref);
				}
			}

			void Save(Entity e, toml::table& entityTable) const override
			{
				if (const Ref* ref = e.TryGet<const Ref>())
				{
					entityTable.insert(Key(), Serializer<Ref>::Serialize(*ref));
				}
			}

		private:
			i32              m_drawOrder;
			std::string_view m_defaultPath;
		};
	}

	toml::table Serializer<MeshRef>::Serialize(const MeshRef& mesh)
	{
		toml::table table;
		WriteAssetRef(table, Cache<GPUMesh>().GetKey(mesh.Handle));
		return table;
	}

	std::optional<MeshRef> Serializer<MeshRef>::Deserialize(const toml::node& node)
	{
		const toml::table* table = node.as_table();
		if (!table) return std::nullopt;
		return MeshRef{ Cache<GPUMesh>().Acquire(Device(), ReadAssetRef(*table, "mesh")) };
	}

	toml::table Serializer<MaterialRef>::Serialize(const MaterialRef& material)
	{
		toml::table table;
		WriteAssetRef(table, Cache<Material>().GetKey(material.Handle));
		return table;
	}

	std::optional<MaterialRef> Serializer<MaterialRef>::Deserialize(const toml::node& node)
	{
		const toml::table* table = node.as_table();
		if (!table) return std::nullopt;
		return MaterialRef{ Cache<Material>().Acquire(Device(), ReadAssetRef(*table, "material")) };
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
			KOI_LOG(ECS, Error, "Component '{}' is already registered; ignoring the second registration", type->Key());
			return;
		}

		KOI_LOG(ECS, Debug, "Registered component '{}' (draw order {})", type->Key(), type->DrawOrder());

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

	void ComponentRegistry::RegisterEngineComponents()
	{
		Register<ComponentType<Transform>>(0, true);
		Register<AssetRefComponentType<MeshRef, GPUMesh>>(10, kDefaultMesh);
		Register<AssetRefComponentType<MaterialRef, Material>>(11, kDefaultMaterial);
		Register<ComponentType<CameraComponent>>(20);
		Register<ComponentType<Light>>(30);
		Register<ComponentType<PostProcess>>(40);
		Register<ComponentType<PhysicsSettings>>(45);
		Register<ComponentType<RigidBody>>(46);
		Register<ComponentType<Collider>>(47);
	}
}
