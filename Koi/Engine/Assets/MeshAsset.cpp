#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Assets/AssetRegistry.h"
#include "Koi/Engine/Assets/AssetTypes.h"
#include "Koi/Engine/Assets/MeshFile.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Runtime/Engine.h"

namespace koi
{
	std::unique_ptr<ModelAsset> AssetTraits<GPUMesh>::Load(const std::string& vfsPath)
	{
		if (AssetFileTypeForPath(vfsPath) != AssetFileType::Mesh)
		{
			KOI_LOG_ERROR("{} isn't a .kmesh. Import the model in the editor and use what it writes", vfsPath);
			return nullptr;
		}

		std::optional<AssetFile> file = AssetFile::Load(vfsPath);
		if (!file) return nullptr;

		auto model      = std::make_unique<ModelAsset>();
		model->Prepared = ReadPreparedMesh(std::move(*file), vfsPath);
		if (!model->Prepared) return nullptr;

		const PreparedMesh& mesh = *model->Prepared;
		model->SourcePath = vfsPath;
		model->Bounds     = BoundingBox(
			Vector3(mesh.Header->BoundsMin) * 0.5f + Vector3(mesh.Header->BoundsMax) * 0.5f,
			Vector3(mesh.Header->BoundsMax) * 0.5f - Vector3(mesh.Header->BoundsMin) * 0.5f);

		// Each slot names its default material by id; the index knows where that file lives today
		const AssetIndex& index = Engine::Get().GetSubsystem<AssetRegistry>().Index();
		for (const MeshMaterialSlot& slot : mesh.Slots)
		{
			const UUID id(slot.Material);
			const AssetIndexEntry* entry = id.is_nil() ? nullptr : index.Find(id);
			if (!id.is_nil() && !entry)
				KOI_LOG_WARN("{}: slot '{}' wants material {}, which isn't in the project anymore", vfsPath, mesh.SlotName(slot), ToString(id));

			model->MaterialBindings.push_back(entry ? AssetKey(entry->VirtualPath) : AssetKey());
		}
		return model;
	}

	GPUMesh AssetTraits<GPUMesh>::Upload(GfxDevice& device, const ModelAsset& source)
	{
		GPUMesh mesh = GPUMesh::Upload(device, source);
		mesh.BindMaterials(device, source);
		return mesh;
	}

	void AssetTraits<GPUMesh>::Destroy(GfxDevice& device, GPUMesh& resource)
	{
		resource.Release(device);
	}
}
