#include "Koi/Engine/Assets/MeshAsset.h"
#include "Koi/Engine/Graphics/MeshLoader.h"
#include "Koi/Engine/Graphics/Procedural/MeshGenerators.h"
#include "Koi/Engine/Graphics/Procedural/ProceduralMeshSpec.h"
#include "Koi/Engine/Memory/MemoryTracker.h"

namespace koi
{
	std::unique_ptr<ModelAsset> AssetTraits<GPUMesh>::Load(const std::string& vfsPath)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		// Generated on demand, so there is no file to resolve and nothing for the cooker to copy
		if (vfsPath.starts_with(kProceduralScheme))
			return LoadProcedural(vfsPath);

		std::unique_ptr<ModelAsset> model = MeshLoader::Load(vfsPath);
		if (model && model->Meshes.empty())
			return nullptr;

		return model;
	}

	GPUMesh AssetTraits<GPUMesh>::Upload(GfxDevice& device, const ModelAsset& source)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		return GPUMesh::Upload(device, source);
	}

	void AssetTraits<GPUMesh>::Destroy(GfxDevice& device, GPUMesh& resource)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		resource.Release(device);
	}
}
