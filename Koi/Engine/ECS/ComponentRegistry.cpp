#include "Koi/Engine/ECS/ComponentRegistry.h"
#include "Koi/Engine/ECS/ComponentInspectors.h"
#include "Koi/Engine/ECS/CameraSerializer.h"
#include "Koi/Engine/ECS/LightSerializer.h"
#include "Koi/Engine/Math/TransformSerializer.h"
#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Runtime/Engine.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include "Koi/Engine/Logging/Logger.h"

namespace koi
{
	namespace
	{
		void LoadMesh(Entity e, const toml::table& entityTable, std::string_view key, GfxDevice& device)
		{
			const toml::table* meshTable = entityTable[key].as_table();
			if (!meshTable) return;

			const std::string path = (*meshTable)["path"].value_or(std::string());
			if (path.empty()) return;

			const MeshHandle handle = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().Acquire(device, path);
			if (handle) e.set<MeshRef>({ handle });
		}

		void SaveMesh(Entity e, toml::table& entityTable, std::string_view key)
		{
			const MeshRef* meshRef = e.try_get<MeshRef>();
			if (!meshRef) return;

			const std::string_view path = Engine::Get().GetSubsystem<AssetRegistry>().Cache<GPUMesh>().GetPath(meshRef->Handle);

			toml::table meshTable;
			meshTable.insert("path", std::string(path));
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

		Register<Transform, true>("transform", DrawTransformInspector);
		Register(ComponentInfo{ .Key = "mesh", .Load = LoadMesh, .Save = SaveMesh, .Draw = DrawMeshInspector });
		Register<CameraComponent>("camera", DrawCameraInspector);
		Register<Light>("light", DrawLightInspector);

		KOI_LOG_DEBUG("Component registry: {} component(s) registered", All().size());
	}
}
