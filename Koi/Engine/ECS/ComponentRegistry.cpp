#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Assets/AssetIdentity.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/ECS/CameraSerializer.h"
#include "Koi/Engine/ECS/LightSerializer.h"
#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/TransformSerializer.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Scripting/ScriptComponent.h"
#include <algorithm>

namespace koi
{
	namespace
	{
		AssetCache<GPUMesh>& Meshes() { return Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>(); }
		GfxDevice&           Device() { return Engine::Get().GetSubsystem<Renderer>().GetDevice(); }

		// [entity.mesh.procedural] is the readable spelling of a "proc://" uri, both end up as the same AssetCache key
		AssetKey ResolveMeshKey(const toml::table& meshTable)
		{
			if (const toml::table* proc = meshTable["procedural"].as_table())
			{
				const std::string generator = (*proc)["generator"].value_or(std::string());
				if (generator.empty()) return {};

				toml::table params = *proc;
				params.erase("generator");
				return AssetKey::FromUri(ToProceduralUri(generator, params));
			}

			const AssetIdentity& identity = AssetIdentity::Get();
			const std::string path   = meshTable["path"].value_or(std::string());
			const std::string idText = meshTable["uuid"].value_or(std::string());

			// A scene without a uuid is hand-authored or pre-identity | use the path instead
			if (idText.empty()) return identity.KeyForPath(path);

			const AssetKey key(UUIDFromString(idText));
			const std::string_view resolved = identity.PathFor(key);
			if (resolved.empty())
			{
				KOI_LOG_WARN("Unresolved asset uuid {}; falling back to path '{}'", idText, path);
				return identity.KeyForPath(path);
			}

			// Expected whenever the asset moved since the scene was written - the uuid wins
			if (!path.empty() && path != resolved)
			{
				KOI_LOG_DEBUG("Asset {} moved: '{}' -> '{}'", idText, path, resolved);
			}

			return key;
		}

		// MeshRef is a cache handle rather than plain data, so it cannot depend on Serializer<T>
		class MeshComponentType final : public IComponentType
		{
		public:
			[[nodiscard]] std::string_view Key() const override { return "mesh"; }
			[[nodiscard]] i32 DrawOrder() const override { return 10; }

			[[nodiscard]] bool Has(Entity e) const override { return e.has<MeshRef>(); }

			// Default to a procedural box so a freshly added component has something visible
			void Add(Entity e) const override
			{
				const MeshGeneratorInfo* box = MeshGeneratorRegistry::Get().Find("box");
				if (!box) return;

				const MeshHandle handle = Meshes().Acquire(Device(), AssetKey::FromUri(ToProceduralUri("box", box->Defaults())));
				if (handle) e.set<MeshRef>({ handle });
			}

			// World's MeshRef on_remove hook hands the cache reference back
			void Remove(Entity e) const override { e.remove<MeshRef>(); }

			void Load(Entity e, const toml::table& entityTable) const override
			{
				const toml::table* meshTable = entityTable[Key()].as_table();
				if (!meshTable) return;

				const AssetKey key = ResolveMeshKey(*meshTable);
				if (!key) return;

				const MeshHandle handle = Meshes().Acquire(Device(), key);
				if (handle) e.set<MeshRef>({ handle });
			}

			void Save(Entity e, toml::table& entityTable) const override
			{
				const MeshRef* meshRef = e.try_get<MeshRef>();
				if (!meshRef) return;

				const AssetKey key = Meshes().GetKey(meshRef->Handle);
				const std::string_view path = AssetIdentity::Get().PathFor(key);

				toml::table meshTable;

				// Procedural meshes round-trip back into the sub-table form rather than the raw URI
				if (std::optional parsed = ParseProceduralUri(path))
				{
					auto& [generator, params] = *parsed;
					params.insert("generator", std::move(generator));
					meshTable.insert("procedural", std::move(params));
				}
				else
				{
					// The uuid is what resolves on load, the path is the readable hint beside it
					meshTable.insert("path", std::string(path));
					if (!key.IsUri()) meshTable.insert("uuid", key.ToString());
				}

				entityTable.insert(Key(), std::move(meshTable));
			}
		};

		class ScriptComponentType final : public IComponentType
		{
		public:
			[[nodiscard]] std::string_view Key() const override { return "script"; }
			[[nodiscard]] i32 DrawOrder() const override { return 40; }

			[[nodiscard]] bool Has(Entity e) const override { return e.has<ScriptComponent>(); }
			void Add(Entity e) const override    { e.set<ScriptComponent>({}); }
			void Remove(Entity e) const override { e.remove<ScriptComponent>(); }

			void Load(Entity e, const toml::table& entityTable) const override { LoadScript(e, entityTable, Key()); }
			void Save(Entity e, toml::table& entityTable) const override       { SaveScript(e, entityTable, Key()); }
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

		registry.Register<ComponentType<Transform>>("transform", 0, true);
		registry.Register<MeshComponentType>();
		registry.Register<ComponentType<CameraComponent>>("camera", 20);
		registry.Register<ComponentType<Light>>("light", 30);
		registry.Register<ScriptComponentType>();
	}
}
