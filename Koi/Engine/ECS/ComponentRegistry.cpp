#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/Assets/AssetIdentity.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Core/Config.h"
#include "Koi/Engine/ECS/CameraSerializer.h"
#include "Koi/Engine/ECS/ComponentInspectors.h"
#include "Koi/Engine/ECS/LightSerializer.h"
#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include "Koi/Engine/Graphics/Renderer.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Math/TransformSerializer.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Scripting/ScriptComponent.h"
#include <algorithm>

namespace koi
{
	namespace
	{
		// [entity.mesh.procedural] is the readable spelling of a "proc://" path; both end up as the same AssetCache key
		std::string ResolveMeshPath(const toml::table& meshTable)
		{
			if (const toml::table* proc = meshTable["procedural"].as_table())
			{
				const std::string generator = (*proc)["generator"].value_or(std::string());
				if (generator.empty()) return {};

				toml::table params = *proc;
				params.erase("generator");
				return ToProceduralUri(generator, params);
			}

			const std::string path   = meshTable["path"].value_or(std::string());
			const std::string idText = meshTable["uuid"].value_or(std::string());

			// A scene without a uuid is hand-authored or pre-identity | user the path instead
			if (idText.empty()) return path;

			const std::string_view resolved = AssetIdentity::Get().PathFor(UUIDFromString(idText));
			if (resolved.empty())
			{
				KOI_LOG_WARN("Unresolved asset uuid {}; falling back to path '{}'", idText, path);
				return path;
			}

			// Expected whenever the asset moved since the scene was written - the uuid wins
			if (!path.empty() && path != resolved)
				KOI_LOG_DEBUG("Asset {} moved: '{}' -> '{}'", idText, path, resolved);

			return std::string(resolved);
		}

		void LoadMesh(Entity e, const toml::table& entityTable, std::string_view key, GfxDevice& device)
		{
			const toml::table* meshTable = entityTable[key].as_table();
			if (!meshTable) return;

			const std::string path = ResolveMeshPath(*meshTable);
			if (path.empty()) return;

			const MeshHandle handle = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().Acquire(device, path);
			if (handle) e.set<MeshRef>({ handle });
		}

		// Add-Component default: a procedural box so the entity has something visible
		void AddMesh(Entity e, GfxDevice& device)
		{
			const MeshGeneratorInfo* box = MeshGeneratorRegistry::Get().Find("box");
			if (!box) return;

			const std::string uri = ToProceduralUri("box", box->Defaults());
			const MeshHandle handle = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().Acquire(device, uri);
			if (handle) e.set<MeshRef>({ handle });
		}

		void RemoveMesh(Entity e)
		{
			if (const MeshRef* meshRef = e.try_get<MeshRef>())
			{
				GfxDevice& device = Engine::Get().GetSubsystem<Renderer>().GetDevice();
				Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().Release(device, meshRef->Handle);
			}
			e.remove<MeshRef>();
		}

		void SaveMesh(Entity e, toml::table& entityTable, std::string_view key)
		{
			const MeshRef* meshRef = e.try_get<MeshRef>();
			if (!meshRef) return;

			const std::string_view path = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().GetPath(meshRef->Handle);

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
				meshTable.insert("path", std::string(path));

#if KOI_WITH_EDITOR
				// The uuid is what resolves on load; the path above stays as the readable hint.
				// Only the editor ever writes a scene, so the reverse lookup lives only here
				if (const UUID id = AssetIdentity::Get().IdFor(path); !id.is_nil())
					meshTable.insert("uuid", ToString(id));
#endif
			}

			entityTable.insert(key, std::move(meshTable));
		}
	}

	ComponentRegistry& ComponentRegistry::Get()
	{
		static ComponentRegistry s_instance;
		return s_instance;
	}

	ComponentRegistry::ComponentRegistry()
	{
		KOI_MEM_TAG_SCOPE(MemTag::ECS);

		Register<Transform, true>("transform", DrawTransformInspector, 0);

		Register(ComponentInfo
		{
			.Key       = "mesh",
			.DrawOrder = 10,
			.Load      = LoadMesh,
			.Save      = SaveMesh,
			.Draw      = DrawMeshInspector,
			.Has       = [](Entity e) { return e.has<MeshRef>(); },
			.Add       = AddMesh,
			.Remove    = RemoveMesh,
		});

		Register<CameraComponent>("camera", DrawCameraInspector, 20);
		Register<Light>("light", DrawLightInspector, 30);

		Register(ComponentInfo
		{
			.Key       = "script",
			.DrawOrder = 40,
			.Load      = LoadScript,
			.Save      = SaveScript,
			.Draw      = DrawScriptInspector,
			.Has       = [](Entity e) { return e.has<ScriptComponent>(); },
			.Add       = [](Entity e, GfxDevice&) { e.set<ScriptComponent>({}); },
			.Remove    = [](Entity e) { e.remove<ScriptComponent>(); },
		});

		// Draw order is explicit per component above, independent of registration order
		std::ranges::stable_sort(m_components, {}, &ComponentInfo::DrawOrder);

		KOI_LOG_DEBUG("Component registry: {} component(s) registered", All().size());
	}
}
