#include "Koi/Engine/Graphics/MeshRegistry.h"
#include "Koi/Engine/Logging/Logger.h"
#include "Koi/Engine/Graphics/MeshLoader.h"
#include "Koi/Engine/Memory/MemoryTracker.h"
#include <cassert>

namespace koi
{
	MeshHandle MeshRegistry::GetOrLoad(const std::string& path, GfxDevice& device, math::AABB* outBoundsIfNew)
	{
		if (auto it = m_pathToHandle.find(path); it != m_pathToHandle.end())
			return it->second;

		KOI_MEM_TAG_SCOPE(MemTag::Assets);

		auto model = MeshLoader::Load(path);
		assert(model && !model->Meshes.empty() && "MeshRegistry: failed to load mesh");

		MeshHandle handle = m_meshes.Emplace(GPUMesh::Upload(device, *model));
		m_pathToHandle.emplace(path, handle);

		if (outBoundsIfNew)
			outBoundsIfNew->Expand(model->Bounds);

		return handle;
	}

	void MeshRegistry::Release(GfxDevice& device)
	{
		KOI_MEM_TAG_SCOPE(MemTag::Assets);
		m_meshes.ForEach([&device](MeshHandle, GPUMesh& mesh) { mesh.Release(device); });
		m_meshes.Reset();
		m_pathToHandle.clear();
	}
}
