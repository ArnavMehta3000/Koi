#pragma once
#include "Koi/Engine/Core/Handle.h"
#include "Koi/Engine/Graphics/GPUMesh.h"
#include "Koi/Engine/Math/Math.h"
#include <string>
#include <unordered_map>

namespace koi
{
	class GfxDevice;

	using MeshHandle = Handle<GPUMesh>;

	// Owns loaded GPUMesh resources, deduplicated by source path.
	class MeshRegistry
	{
	public:
		MeshRegistry() = default;
		MeshRegistry(const MeshRegistry&) = delete;
		MeshRegistry& operator=(const MeshRegistry&) = delete;
		MeshRegistry(MeshRegistry&&) noexcept = default;
		MeshRegistry& operator=(MeshRegistry&&) noexcept = default;

		// Returns the existing handle if `path` was already loaded, otherwise loads + uploads it
		[[nodiscard]] MeshHandle GetOrLoad(const std::string& path, GfxDevice& device, AABB* outBoundsIfNew = nullptr);

		[[nodiscard]] const GPUMesh* Get(MeshHandle handle) const { return m_meshes.Get(handle); }

		void Release(GfxDevice& device);

	private:
		HandlePool<GPUMesh> m_meshes;
		std::unordered_map<std::string, MeshHandle> m_pathToHandle;
	};
}
